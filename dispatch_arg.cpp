#include "dispatch_arg.h"

DispatchArg::DispatchArg(const SSBO* arg_buffer, uint32_t offset, uint32_t direct_value, uint32_t workgroup_size) : 
arg_buffer(arg_buffer), offset(offset), direct_value(direct_value), workgroup_size(workgroup_size) {
    
}