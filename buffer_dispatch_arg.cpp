#include "buffer_dispatch_arg.h"

BufferDispatchArg::BufferDispatchArg(const SSBO* arg_buffer, uint32_t offset, uint32_t workgroup_size) : 
DispatchArg(arg_buffer, offset, 1u, workgroup_size) {

}