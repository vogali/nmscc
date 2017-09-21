#define module cumodule
#include <nms/cuda/cuda.h>
#undef module

#include <nms/test.h>
#include <nms/util/library.h>
#include <nms/cuda/runtime.h>

namespace nms::cuda
{

#pragma region library

#define NMS_CUDA_DEF(F)                                                                                     \
    F(cuGetErrorName),      F(cuDeviceGetCount),        F(cuInit),                                          \
    F(cuCtxCreate_v2),      F(cuCtxSetCurrent),         F(cuCtxSynchronize),                                \
    F(cuStreamCreate),      F(cuStreamDestroy_v2),      F(cuStreamSynchronize),                             \
    F(cuMemAlloc_v2),       F(cuMemFree_v2),            F(cuMemHostAlloc),              F(cuMemFreeHost),   \
    F(cuArray3DCreate_v2),  F(cuArrayDestroy),          F(cuArray3DGetDescriptor_v2),                       \
    F(cuMemcpy),            F(cuMemcpyHtoD_v2),         F(cuMemcpyDtoH_v2),                                 \
    F(cuMemcpyAsync),       F(cuMemcpyHtoDAsync_v2),    F(cuMemcpyDtoHAsync_v2),                            \
    F(cuTexObjectCreate),   F(cuTexObjectDestroy),                                                          \
    F(cuModuleLoadData),    F(cuModuleUnload),                                                              \
    F(cuModuleGetFunction), F(cuModuleGetGlobal_v2),    F(cuLaunchKernel)

enum CudaLibTable
{
#define NMS_CUDA_IDX(name) $##name
    NMS_CUDA_DEF(NMS_CUDA_IDX)
#undef  NMS_CUDA_IDX
};

static auto& cudaLib() {
    static Library  lib("NVCUDA.dll");
    return lib;
}

static auto cudaFun(u32 id) {
    static auto& lib = cudaLib();

    static Library::Function funcs[] = {
#define NMS_CUDA_FUN(name) lib[StrView{#name}]
        NMS_CUDA_DEF(NMS_CUDA_FUN)
#undef  NMS_CUDA_FUN
    };

    if (!lib) {
        NMS_THROW(Exception(CUDA_ERROR_NOT_INITIALIZED));
    }

    auto ret = funcs[id];
    if (!ret) {
        NMS_THROW(Exception(CUDA_ERROR_NOT_INITIALIZED));
    }

    return ret;
}

#define NMS_CUDA_DO(name)   static_cast<decltype(name)*>(cudaFun($##name))

#pragma endregion

#pragma region exception
void Exception::format(String<>& buf) const {
    static auto& lib = cudaLib();
    if (!lib) {
        buf += StrView("nms.cuda: cuda error not initialized");
        return;
    }

    try {
        const char* errmsg = nullptr;
        NMS_CUDA_DO(cuGetErrorName)(static_cast<CUresult>(id_), &errmsg);
        sformat(buf, "nms.cuda: error({}), {}", id_, StrView{ errmsg, u32(strlen(errmsg)) });
    } catch(...)
    {}
}

/**
* log message and emit exception when cuda runtime failes.
*/
template<u32 N>
__forceinline void operator||(cudaError_enum eid,  const char(&msg)[N]) {
    if (eid == cudaError_enum(0)) {
        return;
    }
    io::log::error(msg);
    NMS_THROW(Exception(eid));
}

#pragma endregion

#pragma region device
i32             Device::gid_ = -1;
Device::ctx_t*  Device::ctx_[32];

void driver_init() {
    static const auto stat = [] {
        const auto init_stat = NMS_CUDA_DO(cuInit)(0);
        if (init_stat != CUDA_SUCCESS) {
            return init_stat;
        }

        return CUDA_SUCCESS;
    }();

    if (stat != 0) {
        NMS_THROW(Exception(stat));
    }
}

void device_sync() {
    NMS_CUDA_DO(cuCtxSynchronize)() || "nms.cuda.Device.sync: failed";
}

NMS_API u32 Device::count() {
    driver_init();

    static i32 dev_count = 0;

    static auto static_init = [&] {

        const auto dev_stat = NMS_CUDA_DO(cuDeviceGetCount)(&dev_count);
        if (dev_stat != CUDA_SUCCESS) {
            return dev_stat;
        }

        for (auto i = 0; i < dev_count; ++i) {
            const auto ctx_stat = NMS_CUDA_DO(cuCtxCreate_v2)(&ctx_[i], 0, i);
            if (ctx_stat != CUDA_SUCCESS) {
                io::log::error("nms.cuda: create context failed.");
                return ctx_stat;
            }
        }

        const auto ctx_stat = NMS_CUDA_DO(cuCtxSetCurrent)(ctx_[0]);
        if (ctx_stat != CUDA_SUCCESS) {
            io::log::error("nms.cuda: set context failed.");
            return ctx_stat;
        }

        return CUDA_SUCCESS;
    }();
    (void)static_init;

    return u32(dev_count);
}

NMS_API void Device::sync() const {
    auto oid = gid_;
    auto nid = i32(id_);

    if (oid!=nid) {
        NMS_CUDA_DO(cuCtxSetCurrent)(ctx_[nid]) || "nms.cuda: cannot set context";
    }

    device_sync();

    if (oid!=nid) {
        NMS_CUDA_DO(cuCtxSetCurrent)(ctx_[oid])|| "nms.cuda: cannot set context";
    }
}

#pragma endregion

#pragma region stream
NMS_API void Stream::_new() {
    if (id_ != nullptr) {
        return;
    }
    NMS_CUDA_DO(cuStreamCreate)(&id_, 0) || "nms.cuda.Stream: create stream failed";
}

NMS_API void Stream::_del() {
    if (id_ == nullptr) {
        return;
    }
    NMS_CUDA_DO(cuStreamDestroy_v2)(id_) || "nms.cuda.Stream: destroy stream failed";
}

NMS_API void Stream::sync() const {
    NMS_CUDA_DO(cuStreamSynchronize)(id_) || "nms.cuda.Stream: sync failed";
}

#pragma endregion

#pragma region memory
NMS_API void* _mnew(u64 size) {
    if (size == 0) {
        return nullptr;
    }
    (void)Device::count();

    CUdeviceptr ptr = 0;
    NMS_CUDA_DO(cuMemAlloc_v2)(&ptr, size) || "nms.cuda.mnew: failed.";
    return reinterpret_cast<void*>(ptr);
}

NMS_API void _mdel(void* ptr) {
    if (ptr == nullptr) {
        return;
    }
    NMS_CUDA_DO(cuMemFree_v2)(reinterpret_cast<CUdeviceptr>(ptr)) || "nms.cuda.mdel: failed";
}

NMS_API void* _hnew(u64 size) {
    if (size == 0) {
        return nullptr;
    }
    (void)Device::count();

    void* ptr = nullptr;
    NMS_CUDA_DO(cuMemHostAlloc)(&ptr, size, 0) || "nms.cuda.hnew: failed";
    return ptr;
}

NMS_API void _hdel(void* ptr) {
    if (ptr == nullptr) {
        return;
    }
    NMS_CUDA_DO(cuMemFreeHost)(ptr) || "nms.cuda.hdel: failed";
}

static CUarray_format_enum arr_fmt(char type, u32 size) {
    if (type == 'i') {
        switch (size) {
        case 1: return CU_AD_FORMAT_SIGNED_INT8;
        case 2: return CU_AD_FORMAT_SIGNED_INT16;
        case 4: return CU_AD_FORMAT_SIGNED_INT32;
        default:break;
        }
    }
    if (type == 'u') {
        switch (size) {
        case 1: return CU_AD_FORMAT_UNSIGNED_INT8;
        case 2: return CU_AD_FORMAT_UNSIGNED_INT16;
        case 4: return CU_AD_FORMAT_UNSIGNED_INT32;
        default:break;
        }
    }
    if (type == 'f') {
        switch (size) {
        case 4: return CU_AD_FORMAT_FLOAT;
        default:break;
        }
    }
    NMS_THROW(Exception(CUDA_ERROR_INVALID_VALUE));
}

NMS_API arr_t arr_new(char type, u32 size, u32 channels, u32 rank, const u32 dims[]) {
    arr_t arr = nullptr;
    CUDA_ARRAY3D_DESCRIPTOR desc = {
        rank > 0 ? dims[0] : 0, // width
        rank > 1 ? dims[1] : 0, // height
        rank > 2 ? dims[2] : 0, // depth
        arr_fmt(type, size),    // format
        channels,               // chanels
        0,                      // flags
    };

    const auto stat = NMS_CUDA_DO(cuArray3DCreate_v2)(&arr, &desc);
    void(stat || "nms.cuda.arr_new: cuda array create failed.");

    return arr;
}

NMS_API void arr_del(arr_t arr) {
    const auto stat = NMS_CUDA_DO(cuArrayDestroy)(arr);
    void(stat || "nms.cuda.arr_del: cuda array destory failed.");
}

NMS_API void _mcpy(void* dst, const void* src, u64 size, Stream& stream) {
    if (dst == nullptr || src == nullptr || size == 0) {
        return;
    }
    auto dptr = reinterpret_cast<CUdeviceptr>(dst);
    auto sptr = reinterpret_cast<CUdeviceptr>(src);
    NMS_CUDA_DO(cuCtxSynchronize)() || "nms.cuda.Device.sync: failed";

    auto sid = stream.id();
    auto ret = sid == nullptr
        ? NMS_CUDA_DO(cuMemcpy)(dptr, sptr, size_t(size))
        : NMS_CUDA_DO(cuMemcpyAsync)(dptr, sptr, size_t(size), sid);

    ret || "nms.cuda.mcpy: failed";
}

NMS_API void _h2dcpy(void* dst, const void* src, u64 size, Stream& stream) {
    if (dst == nullptr || src == nullptr || size == 0) {
        return;
    }
    auto sid = stream.id();
    auto ret = sid == nullptr
        ? NMS_CUDA_DO(cuMemcpyHtoD_v2)(reinterpret_cast<CUdeviceptr>(dst), src, size)
        : NMS_CUDA_DO(cuMemcpyHtoDAsync_v2)(reinterpret_cast<CUdeviceptr>(dst), src, size, sid);

    ret || "nms.cuda.h2dcpy: failed";
}

NMS_API void _d2hcpy(void* dst, const void* src, u64 size, Stream& stream) {
    if (dst == nullptr || src == nullptr || size == 0) {
        return;
    }
    auto sid = stream.id();
    auto ret = sid == nullptr
        ? NMS_CUDA_DO(cuMemcpyDtoH_v2)(dst, reinterpret_cast<CUdeviceptr>(src), size)
        : NMS_CUDA_DO(cuMemcpyDtoHAsync_v2)(dst, reinterpret_cast<CUdeviceptr>(src), size, sid);

    ret || "nms.cuda.d2hcpy: failed";
}

#pragma endregion

#pragma region texture

static CUresourceViewFormat_enum mkTexFormat(CUarray_format_enum fmt, u32 n) {
    using T = CUresourceViewFormat_enum;

    const auto offset = n == 1 ? 0 : n == 2 ? 1 : n == 4 ? 2 : 0;

    switch (fmt) {
    case CU_AD_FORMAT_SIGNED_INT8:      return T(offset + CU_RES_VIEW_FORMAT_SINT_1X8  );
    case CU_AD_FORMAT_SIGNED_INT16:     return T(offset + CU_RES_VIEW_FORMAT_SINT_1X16 );
    case CU_AD_FORMAT_SIGNED_INT32:     return T(offset + CU_RES_VIEW_FORMAT_SINT_1X32 );
    case CU_AD_FORMAT_UNSIGNED_INT8:    return T(offset + CU_RES_VIEW_FORMAT_UINT_1X8  );
    case CU_AD_FORMAT_UNSIGNED_INT16:   return T(offset + CU_RES_VIEW_FORMAT_UINT_1X16 );
    case CU_AD_FORMAT_UNSIGNED_INT32:   return T(offset + CU_RES_VIEW_FORMAT_UINT_1X32 );
    case CU_AD_FORMAT_FLOAT:            return T(offset + CU_RES_VIEW_FORMAT_FLOAT_1X32);

    case CU_AD_FORMAT_HALF:
    default: break;
    }

    return CU_RES_VIEW_FORMAT_NONE;
}

NMS_API u64 tex_new(arr_t arr, TexAddressMode addres_mode, TexFilterMode filter_mode) {
    CUtexObject tex = 0;

    CUDA_ARRAY3D_DESCRIPTOR arr_desc = {};
    CUDA_RESOURCE_DESC      res_desc = {};
    CUDA_TEXTURE_DESC       tex_desc = {};
    CUDA_RESOURCE_VIEW_DESC view_desc = {};

    res_desc.resType = CU_RESOURCE_TYPE_ARRAY;
    res_desc.res.array.hArray = arr;

    tex_desc.addressMode[0] = CUaddress_mode(addres_mode);
    tex_desc.addressMode[1] = CUaddress_mode(addres_mode);
    tex_desc.addressMode[2] = CUaddress_mode(addres_mode);
    tex_desc.filterMode = CUfilter_mode(filter_mode);

    NMS_CUDA_DO(cuArray3DGetDescriptor_v2)(&arr_desc, arr);
    view_desc.format = mkTexFormat(arr_desc.Format, arr_desc.NumChannels);
    view_desc.width = arr_desc.Width;
    view_desc.height = arr_desc.Height;
    view_desc.depth = arr_desc.Depth;

    const auto stat = NMS_CUDA_DO(cuTexObjectCreate)(&tex, &res_desc, &tex_desc, &view_desc);
    void(stat || "nms.cuda.tex_new: cuda texture object create failed");

    return tex;
}

NMS_API void tex_del(u64 obj) {
    const auto stat = NMS_CUDA_DO(cuTexObjectDestroy)(obj);
    void(stat || "nms.cuda.tex_del: cuda texture object destroy failed");
}

#pragma endregion

#pragma region module
NMS_API Module::Module(StrView ptx)
    : module_(nullptr)
{
    driver_init();

    auto eid = NMS_CUDA_DO(cuModuleLoadData)(&module_, ptx.data());
    if (eid != 0) {
        io::log::error("nms.cuda.Module.build: load ptx failed.");
    }
}

NMS_API Module::~Module() {
    if (module_ == nullptr) {
        return;
    }

    NMS_CUDA_DO(cuModuleUnload)(module_);
    module_ = nullptr;
}

NMS_API Module::sym_t Module::get_symbol(StrView name) const {
    if (module_ == nullptr) {
        return nullptr;
    }

    auto cname = name.data();

    CUdeviceptr ptr = 0;
    size_t      size = 0;
    const auto  ret = NMS_CUDA_DO(cuModuleGetGlobal_v2)(&ptr, &size, module_, cname);
    if (ret != 0 || ptr == 0) {
        io::log::error("nms.cuda.Module.getArg: cannot get arg => cufun_{}_{}", name);
        NMS_THROW(Exception(ret));
    }
    return reinterpret_cast<sym_t>(ptr);
}

NMS_API void Module::set_symbol(sym_t data, const void* value, u32 size) const {
    _h2dcpy(data, value, size, Stream::global());
}


NMS_API Module::fun_t Module::get_kernel(StrView name) const {
    if (module_ == nullptr) {
        return nullptr;
    }

    auto cname = name.data();

    CUfunc_st* fun = nullptr;
    auto ret = NMS_CUDA_DO(cuModuleGetFunction)(&fun, module_, cname);
    if (ret != 0) {
        io::log::error("nms.cuda.Program.get_kernel: cannot get {}", name);
        NMS_THROW(Exception(ret));
    }

    return reinterpret_cast<fun_t>(fun);
}


NMS_API void Module::run_kernel(fun_t kernel, u32 rank, const u32 dims[], const void* kernel_args[], Stream& stream) const {
    u32 block_dim[3] = {
        rank == 1 ? min(256u, dims[0]) : rank == 2 ? 16u : rank == 3 ? 8u : 8u,
        rank == 1 ? min(001u, dims[0]) : rank == 2 ? 16u : rank == 3 ? 8u : 8u,
        rank == 1 ? min(001u, dims[0]) : rank == 2 ? 01u : rank == 3 ? 8u : 8u,
    };

    u32 grid_dim[3] = {
        rank > 0 ? (dims[0] + block_dim[0] - 1) / block_dim[0] : 1,
        rank > 1 ? (dims[1] + block_dim[1] - 1) / block_dim[1] : 1,
        rank > 2 ? (dims[2] + block_dim[2] - 1) / block_dim[2] : 1
    };

    const auto shared_mem_bytes   = 0;
    const auto h_stream           = stream.id();
    const auto extra              = nullptr;

    const auto stat = NMS_CUDA_DO(cuLaunchKernel)(kernel, grid_dim[0], grid_dim[1], grid_dim[2], block_dim[0], block_dim[1], block_dim[2], shared_mem_bytes, h_stream, const_cast<void**>(kernel_args), extra);
    void(stat || "nms.cuda.invoke: failed.");
}

#pragma endregion

}
