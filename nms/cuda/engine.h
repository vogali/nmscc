#pragma once

#include <nms/cuda/base.h>
#include <nms/cuda/runtime.h>

struct _nvrtcProgram;

namespace nms::cuda
{

struct Vrun;

/*! cuda program */
class Program
{
    friend class  Module;
    friend struct Vrun;

public:
    /**
     * constructor
     * @param source: cuda source (*.cu)
     */
    Program(StrView src)
        : src_{ src }
    {}

    Program()
        : src_{}
    { }

    ~Program()
    { }

    /*! complile cuda source to ptx */
    NMS_API bool compile();

    /*! get cuda src */
    String<>& src() {
        return src_;
    }

    /*! get cuda src */
    const String<>& src() const {
        return src_;
    }

    /*! get cuda ptx */
    const String<>& ptx() const {
        return ptx_;
    }

protected:
    String<>  src_;
    String<>  ptx_;
};

}

