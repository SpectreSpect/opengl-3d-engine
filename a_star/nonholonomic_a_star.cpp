#include "nonholonomic_a_star.h"


namespace rs_detail {

static constexpr double PI = 3.1415926535897932384626433832795;
static constexpr double EPS = 1e-10;

static inline double wrap_pi(double a) {
    return std::atan2(std::sin(a), std::cos(a)); // [-pi, pi]
}

static inline double mod2pi(double a) {
    // keep in [-pi, pi] (works fine for the formulas we use)
    return wrap_pi(a);
}

static inline void polar(double x, double y, double &r, double &theta) {
    r = std::hypot(x, y);
    theta = std::atan2(y, x);
}

enum Seg : int { L = 0, R = 1, S = 2, N = 3 };

struct Candidate {
    std::array<Seg,5> type {N,N,N,N,N};
    std::array<double,5> len {0,0,0,0,0}; // signed (sign => forward/back)
    double total = std::numeric_limits<double>::infinity();
};

static inline double total_len(const Candidate& c) {
    double s = 0.0;
    for (int i=0;i<5;i++) {
        if (c.type[i] == N) break;
        s += std::abs(c.len[i]);
    }
    return s;
}

static inline void try_update(Candidate& best, const std::array<Seg,5>& type,
                              const std::array<double,5>& len)
{
    Candidate c;
    c.type = type;
    c.len  = len;
    c.total = total_len(c);
    if (c.total + 1e-12 < best.total) best = c;
}

// --- Primitive solvers (same math as standard RS derivations) ---

// L+ S+ L+  (works with signed lengths via transforms outside)
static inline bool LpSpLp(double x, double y, double phi, double& t, double& u, double& v) {
    double xi  = x - std::sin(phi);
    double eta = y - 1.0 + std::cos(phi);
    polar(xi, eta, u, t);
    v = mod2pi(phi - t);
    return (t >= -EPS) && (u >= -EPS) && (v >= -EPS);
}

// L+ S+ R+
static inline bool LpSpRp(double x, double y, double phi, double& t, double& u, double& v) {
    double xi  = x + std::sin(phi);
    double eta = y - 1.0 - std::cos(phi);
    double r, theta;
    polar(xi, eta, r, theta);
    if (r < 2.0 - EPS) return false;
    u = std::sqrt(std::max(0.0, r*r - 4.0));
    double ang = std::atan2(2.0, u);
    t = mod2pi(theta + ang);
    v = mod2pi(t - phi);
    return (t >= -EPS) && (u >= -EPS) && (v >= -EPS);
}

// L+ R- L+   (CCC family core)
static inline bool LpRmL(double x, double y, double phi, double& t, double& u, double& v) {
    // One common analytic form (covers the usual CCC cases).
    // Note: u will be negative here (R-).
    double xi  = x - std::sin(phi);
    double eta = y - 1.0 + std::cos(phi);
    double r, theta;
    polar(xi, eta, r, theta);
    if (r > 4.0 + EPS) return false;

    u = -2.0 * std::asin(std::clamp(r / 4.0, -1.0, 1.0)); // negative
    t = mod2pi(theta + 0.5*u + PI);
    v = mod2pi(phi - t + u);
    return (t >= -EPS) && (v >= -EPS);
}

// --- Families: build candidates using timeflip/reflect symmetries ---

static inline void CSC(double x, double y, double phi, Candidate& best) {
    double t,u,v;

    // LSL
    if (LpSpLp(x, y, phi, t,u,v)) {
        try_update(best, {L,S,L,N,N}, { t, u, v, 0,0 });
    }
    // timeflip (reverse time) -> negate lengths
    if (LpSpLp(-x, y, -phi, t,u,v)) {
        try_update(best, {L,S,L,N,N}, { -t, -u, -v, 0,0 });
    }
    // reflect (mirror) swaps L<->R
    if (LpSpLp(x, -y, -phi, t,u,v)) {
        try_update(best, {R,S,R,N,N}, { t, u, v, 0,0 });
    }
    // timeflip + reflect
    if (LpSpLp(-x, -y, phi, t,u,v)) {
        try_update(best, {R,S,R,N,N}, { -t, -u, -v, 0,0 });
    }

    // LSR
    if (LpSpRp(x, y, phi, t,u,v)) {
        try_update(best, {L,S,R,N,N}, { t, u, v, 0,0 });
    }
    if (LpSpRp(-x, y, -phi, t,u,v)) {
        try_update(best, {L,S,R,N,N}, { -t, -u, -v, 0,0 });
    }
    // reflect => RSL
    if (LpSpRp(x, -y, -phi, t,u,v)) {
        try_update(best, {R,S,L,N,N}, { t, u, v, 0,0 });
    }
    if (LpSpRp(-x, -y, phi, t,u,v)) {
        try_update(best, {R,S,L,N,N}, { -t, -u, -v, 0,0 });
    }
}

static inline void CCC(double x, double y, double phi, Candidate& best) {
    double t,u,v;

    // L R L  (with middle being R- via u<0)
    if (LpRmL(x, y, phi, t,u,v)) {
        try_update(best, {L,R,L,N,N}, { t, u, v, 0,0 });
    }
    if (LpRmL(-x, y, -phi, t,u,v)) {
        try_update(best, {L,R,L,N,N}, { -t, -u, -v, 0,0 });
    }
    // reflect => R L R
    if (LpRmL(x, -y, -phi, t,u,v)) {
        try_update(best, {R,L,R,N,N}, { t, u, v, 0,0 });
    }
    if (LpRmL(-x, -y, phi, t,u,v)) {
        try_update(best, {R,L,R,N,N}, { -t, -u, -v, 0,0 });
    }
}

static inline Candidate solve(double x, double y, double phi) {
    Candidate best;
    CSC(x,y,phi,best);
    CCC(x,y,phi,best);
    return best;
}

// Integrate one small signed step ds (unit radius) for segment type.
static inline void step_local(Seg seg, double ds, double& x, double& y, double& yaw) {
    if (std::abs(ds) < 1e-15) return;

    if (seg == S) {
        x += ds * std::cos(yaw);
        y += ds * std::sin(yaw);
        return;
    }

    int k = (seg == L) ? +1 : -1; // curvature sign
    double yaw0 = yaw;
    double yaw1 = yaw0 + k * ds;

    // x += (sin(yaw1)-sin(yaw0))/k ; y += (-cos(yaw1)+cos(yaw0))/k
    x += (std::sin(yaw1) - std::sin(yaw0)) / double(k);
    y += (-std::cos(yaw1) + std::cos(yaw0)) / double(k);
    yaw = mod2pi(yaw1);
}

} // namespace rs_detail


// ------------------------------------------------------------
// Your requested function signature.
// Set min_turn_radius + step to your liking inside.
// ------------------------------------------------------------
std::vector<NonholonomicPos> NonholonomicAStar::find_reeds_shepp(NonholonomicPos start_pos, NonholonomicPos end_pos)
{
    // TODO: set this to your car’s minimum turning radius (world units).
    // If you have wheel_base and max_steer (radians): rho = wheel_base / tan(max_steer)
    const float tanMax = std::tan(max_steer);
    if (std::abs(tanMax) < 1e-6f) return {};    // can't turn -> no RS

    const float min_turn_radius = wheel_base / tanMax;

    // Sampling resolution along the path (world units)
    const float step_world = reeds_shepp_step_world;

    // Work in XZ plane (like your simulator). y is kept from start_pos.
    glm::vec3 d = end_pos.pos - start_pos.pos;

    double th0 = (double)start_pos.theta;
    double c0  = std::cos(th0);
    double s0  = std::sin(th0);

    // Rotate world delta into start frame and normalize by rho
    double dx = (double)d.x;
    double dy = (double)d.z; // Z acts like "y" in 2D formulas

    double x = ( c0*dx + s0*dy ) / (double)min_turn_radius;
    double y = (-s0*dx + c0*dy ) / (double)min_turn_radius;
    double phi = rs_detail::mod2pi((double)end_pos.theta - th0);

    // Solve (local, unit radius)
    rs_detail::Candidate best = rs_detail::solve(x, y, phi);
    if (!std::isfinite(best.total)) return {};

    // Sample the best path in local frame, then transform back to world.
    std::vector<NonholonomicPos> out;
    out.reserve((size_t)(best.total * (double)min_turn_radius / (double)step_world) + 8);

    double lx = 0.0, ly = 0.0, yaw = 0.0;

    auto emit_world = [&](rs_detail::Seg seg, double ds_signed)
    {
        NonholonomicPos p;
        // inverse rotation + scale back:
        // world_dx = cos(th0)*lx - sin(th0)*ly
        // world_dz = sin(th0)*lx + cos(th0)*ly
        double wdx = (c0*lx - s0*ly) * (double)min_turn_radius;
        double wdz = (s0*lx + c0*ly) * (double)min_turn_radius;

        p.pos   = start_pos.pos + glm::vec3((float)wdx, 0.0f, (float)wdz);
        p.pos.y = start_pos.pos.y; // keep altitude; change if you want

        p.theta = (float)rs_detail::mod2pi(th0 + yaw);

        // steer convention: -1 left, +1 right, 0 straight
        if (seg == rs_detail::L) p.steer = -1.0f;
        else if (seg == rs_detail::R) p.steer = +1.0f;
        else p.steer = 0.0f;

        p.dir = (ds_signed >= 0.0) ? 1.0f : -1.0f;

        out.push_back(p);
    };

    // include the start pose
    {
        NonholonomicPos p = start_pos;
        p.steer = 0.0f;
        p.dir   = 1.0f;
        out.push_back(p);
    }

    const double step_u = (double)step_world / (double)min_turn_radius;

    for (int i = 0; i < 5; ++i) {
        if (best.type[i] == rs_detail::N) break;
        rs_detail::Seg seg = best.type[i];
        double L = best.len[i];
        if (std::abs(L) < 1e-12) continue;

        double sgn = (L >= 0.0) ? 1.0 : -1.0;
        double rem = std::abs(L);

        while (rem > 1e-12) {
            double ds = std::min(step_u, rem);
            rem -= ds;
            ds *= sgn; // signed step

            rs_detail::step_local(seg, ds, lx, ly, yaw);
            emit_world(seg, ds);
        }
    }

    // Force the last pose to match the requested end (nice for debugging)
    if (!out.empty()) {
        out.back().pos   = end_pos.pos;
        out.back().theta = end_pos.theta;
    }
    return out;
}



NonholonomicAStar::NonholonomicAStar(VoxelGrid* voxel_grid) {
    this->grid = new VoxelOccupancyGrid3D(voxel_grid);
}

std::vector<NonholonomicPos> NonholonomicAStar::simulate_motion(NonholonomicPos start, int steer, int direction)
{
    steer = std::clamp(steer, -1, 1);
    direction = (direction < 0) ? -1 : 1; // only -1 or +1

    // steer = -1 => max LEFT, steer = +1 => max RIGHT
    float delta = 0.0f;
    if (steer != 0) delta = -steer * max_steer;

    std::vector<NonholonomicPos> out;
    out.reserve(integration_steps);

    glm::vec3 p = start.pos;
    float yaw   = start.theta;

    const float ds = motion_simulation_dist / float(integration_steps);
    const float ds_signed = ds * float(direction);
    const float eps = 1e-6f;

    for (int i = 0; i < integration_steps; ++i) {
        if (std::abs(delta) < eps) {
            // straight (forward or reverse)
            p.x += ds_signed * std::cos(yaw);
            p.z += ds_signed * std::sin(yaw);
        } else {
            // exact circular arc integration (forward or reverse)
            float tanD = std::tan(delta);
            // (tanD won't be ~0 here because of the eps check above)
            float R = wheel_base / tanD;   // signed radius
            float yaw0 = yaw;
            float dYaw = ds_signed / R;    // signed
            yaw += dYaw;

            p.x += R * (std::sin(yaw) - std::sin(yaw0));
            p.z += R * (-std::cos(yaw) + std::cos(yaw0));
        }

        NonholonomicPos s = start;
        s.pos = p;
        s.theta = yaw;
        out.push_back(s);
    }

    return out;
}

// static std::vector<NonholonomicPos> find_reeds_shepp(NonholonomicPos start_pos, NonholonomicPos end_pos) {

// }

bool NonholonomicAStar::adjust_and_check_path(std::vector<NonholonomicPos>& path, int max_step_up, int max_drop) {
    // for (auto& s : path) {
    //     glm::ivec3 voxel_pos = glm::ivec3(glm::floor(s.pos));
    //     if (!adjust_to_ground(voxel_pos, max_step_up, max_drop))
    //         return false;

    //     // IMPORTANT: depending on your convention you may want +0.5f or +1.0f here
    //     s.pos.y = (float)voxel_pos.y;
    // }
    if (path.size() > 0)
    for (int i = 0; i < path.size(); i++) {
        glm::ivec3 voxel_pos = glm::ivec3(glm::floor(path[i].pos));
        if (!adjust_to_ground(voxel_pos, max_step_up, max_drop))
            return false;

        // IMPORTANT: depending on your convention you may want +0.5f or +1.0f here
        path[i].pos.y = (float)voxel_pos.y;
        if (i + 1 < path.size())
            path[i + 1].pos.y = path[i].pos.y;
    }
    return true;
}


bool NonholonomicAStar::adjust_to_ground(glm::ivec3& voxel_pos, int max_step_up, int max_drop) {
    auto solid = [&](const glm::ivec3& q) {
        return grid->get_cell(q).solid;
    };

    // 1) If we're inside solid, try stepping up
    if (solid(voxel_pos)) {
        bool freed = false;
        for (int k = 1; k <= max_step_up; ++k) {
            glm::ivec3 up = voxel_pos + glm::ivec3(0, k, 0);
            if (!solid(up)) {
                voxel_pos = up;
                freed = true;
                break;
            }
        }
        if (!freed) return false;
    }

    // 2) Now find a y such that: current is empty AND below is solid
    // (and don't drop more than max_drop)
    for (int drop = 0; drop <= max_drop; ++drop) {
        if (!solid(voxel_pos) && solid(voxel_pos + glm::ivec3(0, -1, 0)))
            return true;

        // If we somehow are in solid, we're already too low → reject
        // (or you could step up 1, but reject is safer)
        if (solid(voxel_pos))
            return false;

        voxel_pos.y -= 1;
    }

    return false;
}

bool NonholonomicAStar::almost_equal(NonholonomicPos a, NonholonomicPos b) {
    float dist = glm::distance(a.pos, b.pos);
    float theta_diff = std::abs(angle_diff(a.theta, b.theta));

    std::cout << theta_diff << std::endl;

    bool almost_equal_pos = dist <= 0.5f;
    bool almost_equal_angle = theta_diff <= 0.2f;

    return almost_equal_pos && almost_equal_angle;
}

std::vector<NonholonomicPos> NonholonomicAStar::reconstruct_path(std::unordered_map<uint64_t, NonholonomicAStarCell> closed_heap, NonholonomicPos pos) {
    std::vector<NonholonomicPos> path;
    path.push_back(pos);
    NonholonomicPos cur_pos = pos;

    while (true) {
        uint64_t cur_key = NonholonomicKeyPacker::pack(cur_pos);
        auto it = closed_heap.find(cur_key);

        if (it == closed_heap.end())
            return {};
            
        NonholonomicAStarCell prev_cell = it->second;

        path.push_back(cur_pos);
        
        if (prev_cell.no_parent)
            break;
        
        cur_pos = prev_cell.came_from;
    }

    std::reverse(path.begin(), path.end());

    return path;
}


std::vector<NonholonomicPos> NonholonomicAStar::find_nonholomic_path(NonholonomicPos start_pos, NonholonomicPos end_pos) {
    std::priority_queue<NonholonomicAStarCell, std::vector<NonholonomicAStarCell>, NonholonomicByPriority> pq;
    std::unordered_map<uint64_t, NonholonomicAStarCell> closed_heap;
    std::unordered_map<uint64_t, float> g_score;

    NonholonomicAStarCell start;
    start.pos = start_pos;
    start.no_parent = true;
    start.g = 0;
    start.f = 0;

    int limit = 10000;
    int counter = 0;

    pq.push(start);

    while (!pq.empty()) {
        NonholonomicAStarCell cur_cell = pq.top();
        pq.pop();

        if (counter >= limit) {
            std::cout << "Limit exceeded" << std::endl;
            return {};
        }
        // std::cout << "KEK" << std::endl;

        // std::cout << "(" << cur_cell.pos.pos.x << ", " << cur_cell.pos.pos.y << ", " << cur_cell.pos.pos.z << ", " << cur_cell.pos.theta  << ")" << std::endl;
            

        
        uint64_t cur_key = NonholonomicKeyPacker::pack(cur_cell.pos);
        closed_heap[cur_key] = cur_cell;

        // if (cur_cell.pos == end_pos) {
        if (almost_equal(cur_cell.pos, end_pos)) {
            // std::cout << "cur_cell: " << cur_cell.pos.theta << std::endl;
            // std::cout << "end_pos: " << end_pos.theta << std::endl;
            // print_vec();

            std::vector<NonholonomicPos> a_star_path = reconstruct_path(closed_heap, cur_cell.pos);

            std::vector<NonholonomicPos> reeds_shepp_path = find_reeds_shepp(cur_cell.pos, end_pos);

            if (reeds_shepp_path.size() > 0)
                if (adjust_and_check_path(reeds_shepp_path))
                    a_star_path.insert(a_star_path.end(), reeds_shepp_path.begin(), reeds_shepp_path.end());

            return a_star_path;
        }

        if (counter % try_reeds_shepp_interval == 0) {
            std::vector<NonholonomicPos> reeds_shepp_path = find_reeds_shepp(cur_cell.pos, end_pos);

            if (reeds_shepp_path.size() > 0) {
                if (adjust_and_check_path(reeds_shepp_path)) {
                    std::vector<NonholonomicPos> a_star_path = reconstruct_path(closed_heap, cur_cell.pos);
                    a_star_path.insert(a_star_path.end(), reeds_shepp_path.begin(), reeds_shepp_path.end()); // concatenate
                    
                    return a_star_path;
                } 
            }
        }
            
        for (int dir = -1; dir <= 1; dir += 2)
            for (int steer = -1; steer <= 1; steer++) {
                std::vector<NonholonomicPos> motion = NonholonomicAStar::simulate_motion(cur_cell.pos, steer, dir);

                NonholonomicPos new_pos = motion[motion.size() - 1];

                bool need_continue = false;
                float last_y = new_pos.pos.y;
                for (int i = 0; i < motion.size(); i++) {
                    NonholonomicPos intermidiate_pos = motion[i];
                    glm::ivec3 vecpos = glm::ivec3(glm::floor(intermidiate_pos.pos));
                    intermidiate_pos.pos.y = last_y;

                    if (!adjust_to_ground(vecpos)) {
                        need_continue = true;
                        break;
                    }
                        
                    intermidiate_pos.pos.y = vecpos.y;
                }

                if (need_continue)                    
                    continue;

                uint64_t new_key = NonholonomicKeyPacker::pack(new_pos);
                auto heap_it = closed_heap.find(new_key);
                if (heap_it != closed_heap.end()) 
                    continue;
                

                float new_g = cur_cell.g + glm::distance((glm::vec3)cur_cell.pos.pos, (glm::vec3)new_pos.pos);

                auto it = g_score.find(new_key);
                
                if (it != g_score.end()) {
                    float old_g = it->second;
                    if (old_g <= new_g)
                        continue;
                }

                g_score[new_key] = new_g;

                NonholonomicAStarCell new_cell;
                new_cell.pos = new_pos;
                new_cell.pos.steer = steer;
                new_cell.pos.dir = dir;
                // new_cell.pos.motion = motion;
                new_cell.came_from = cur_cell.pos;
                new_cell.no_parent = false;
                new_cell.g = new_g;
                new_cell.f = new_g + get_heuristic(new_pos.pos, end_pos.pos);

                pq.push(new_cell);
            }
            counter++;
    }
    return {};
}