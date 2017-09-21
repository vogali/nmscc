#pragma once

#include <nms/core/view.h>
#include <nms/core/memory.h>
#include <nms/core/exception.h>

namespace nms
{

namespace io
{
class File;
class Path;
}

template<class T, u32 Icapicity = 0>
class List;

template<class T>
class List<T, 0>: public View<T>
{

public:
    using base  = View<T>;
    using Tdata = typename base::Tdata;
    using Tsize = typename base::Tsize;
    using Tinfo = typename base::Tinfo;

#pragma region constructor
    constexpr List() noexcept
    {}

    template<class ...U>
    explicit List(Tsize count, U&& ...us) {
        appends(count, fwd<U>(us)...);
    }

    template<class U, class = $when_is<U, Tdata> >
    List(const View<U>& view)
        : List{} {
        appends(view.data(), view.count());
    }

    template<class U, class = $when_is<U, Tdata> >
    List(const View<const U>& view)
        : List{} {
        appends(view.data(), view.count());
    }

    template<u32 SN >
    List(const Tdata(&view)[SN]) {
        *this +=(view);
    }

    ~List() {
        if (data_ == nullptr) {
            return;
        }

        for(Tsize i = 0; i < size_; ++i) {
            data_[i].~Tdata();
        }

        if (!use_buff()) {
            mdel(data_);
        }

        size_ = 0;
        data_ = nullptr;
    }

    List(List&& rhs) noexcept {
        if (rhs.use_buff()) {
            const auto newcnt = rhs.count();
            reserve(newcnt);
            size_ = newcnt;
            mmov(data(), rhs.data(), newcnt);
        }
        else {
            base::operator=(rhs);
        }
        rhs.data_ = nullptr;
        rhs.size_ = 0;
    }

    List(const List& rhs) noexcept
        : List{}
    {
        appends(rhs.data(), rhs.count());
    }

    List& operator=(List&& rhs) noexcept {
        if (rhs.use_buff()) {
            clear();

            auto newcnt = rhs.count();
            reserve(newcnt);
            mmov(data(), rhs.data(), newcnt);
            size_ = newcnt;
        }
        else {
            base::operator=(rhs);
        }
        rhs.data_ = nullptr;
        rhs.size_ = 0;
        return *this;
    }

    List& operator=(const List& rhs) noexcept {
        clear();
        appends(rhs.data(), rhs.count());
        return *this;
    }

    List& operator=(const View<const Tdata>& rhs) noexcept {
        clear();
        appends(rhs.data(), rhs.count());
        return *this;
    }

    List& operator=(const View<Tdata>& rhs) noexcept {
        clear();
        appends(rhs.data(), rhs.count());
        return *this;
    }

    operator View<const Tdata>() const noexcept {
        return { data_, size_ };
    }
#pragma endregion

#pragma region property
    using base::data;
    using base::size;
    using base::count;

    /* the number of elements that can be held in currently allocated storage */
    Tsize capacity() const noexcept {
        return capacity_;
    }
#pragma endregion

#pragma region method
    List& reserve(Tsize newcnt) {
        const auto oldcnt = base::size_;
        const auto oldcap = capacity_;
        if (newcnt > oldcap) {
            // caculate new capicity
            if (newcnt < oldcnt + oldcnt / 8) {
                newcnt = oldcnt + oldcnt / 8;
            }
            const auto newcap = (newcnt +31) / 32 * 32;
            const auto olddat = data_;

            // realloc
            const auto newdat = mnew<Tdata>(newcap);


            // move olddat -> newdat
            for(Tsize i = 0; i < oldcnt; ++i) {
                void* addr = &newdat[i];
                new (addr)Tdata(static_cast<Tdata&&>(olddat[i]));
            }

            // try: free old data
            if (olddat!=nullptr && !use_buff()) {
                mdel(olddat);
            }

            // update
            data_       = newdat;
            capacity_   = newcap;
        }
        return *this;
    }

    template<class Isize>
    void resize(Isize newsize) {
        static_assert($is<$pod, Tdata>, "nms.List.resize: Tdata shold be POD type");
        reserve(Tsize(newsize));
        size_ = Tsize(newsize);
    }

    void clear() {
        // clear
        for (Tsize i = 0; i < size_; ++i) {
            data_[i].~Tdata();
        }
        size_ = 0;
    }

#pragma endregion

#pragma region append
    /*! append elements to the end */
    template<class ...U>
    List& _append(U&& ...u) {
        new(&data_[size_]) Tdata(fwd<U>(u)...);
        size_ = size_ + 1;
        return *this;
    }

    /*! append elements to the end */
    template<class ...U>
    List& append(U&& ...u) {
        reserve(size_ + 1);
        _append(fwd<U>(u)...);
        return *this;
    }

    /*! append elements to the end */
    template<class ...U>
    List& _appends(Tsize cnt, U&& ...u) {
        for (Tsize i = 0; i < cnt; ++i) {
            new(&data_[base::size_++])Tdata(fwd<U>(u)...);
        }
        return *this;
    }

    /*! append elements to the end */
    template<class ...U>
    List& appends(Tsize cnt, U&& ...u) {
        reserve(size_ + cnt);
        _appends(cnt, fwd<U>(u)...);
        return *this;
    }

    /*! append(copy) elements to the end */
    template<class U>
    List& _appends(const U dat[], Tsize cnt) {
        for (Tsize i = 0; i < cnt; ++i) {
            new(&data_[base::size_++])Tdata(dat[i]);
        }

        return *this;
    }

    /*! append(copy) elements to the end */
    template<class U>
    List& appends(const U dat[], Tsize cnt) {
        reserve(size_ + cnt);
        _appends(dat, cnt);
        return *this;
    }

#pragma endregion

#pragma region operator+=
    template<class U, class = $when_as<Tdata, U> >
    List& operator+=(const View<const U>& rhs) {
        appends(rhs.data(), rhs.count());
        return *this;
    }

    template<class U, class = $when_as<Tdata, U> >
    List& operator+=(const View<U>& rhs) {
        appends(rhs.data(), rhs.count());
        return *this;
    }

    template<u32 SN>
    List& operator+=(const Tdata(&rhs)[SN]) {
        if ($is<char, Tdata>) {
            appends(rhs, SN-1);
        }
        else {
            appends(rhs, SN);
        }
        return *this;
    }

    template<class U, class=$when_as<Tdata, U> >
    List& operator+= (U&& u) {
        append(fwd<U>(u));
        return *this;
    }
#pragma endregion

#pragma region save/load
    void save(io::File& file) const {
        saveFile(*this, file);
    }

    static List load(const io::File& file) {
        List list;
        loadFile(list, file);
        return list;
    }
#pragma endregion

protected:
    using   base::data_;
    using   base::size_;
    using   base::capacity_;


    bool use_buff() const {
        const auto pthis = reinterpret_cast<uintptr_t>(&capacity_);
        const auto pdata = reinterpret_cast<uintptr_t>(data_);
        return (pdata - pthis) < 16*1024*1024;  // 16MB
    }

    template<class File>
    static void saveFile(const List& list, File& file) {
        const auto info = list.info();
        const auto dims = list.size();
        const auto data = list.data();
        const auto nums = list.count();
        file.write(&info, 1);
        file.write(&dims, 1);
        file.write(data, nums);
    }

    template<class File>
    static void loadFile(List& list, const File& file) {

        Tinfo info;
        file.read(&info, 1);
        if (info != base::$info) {
            NMS_THROW(Eunexpect<Tinfo>(base::$info, info));
        }

        typename base::Tdims dims;
        file.read(&dims, 1);

        list.resize(dims[0]);
        file.read(list.data(), dims[0]);
    }
};

template<class T, u32 Icapicity>
class List: public List<T, 0>
{
public:
    using base  = List<T, 0>;
    using Tdata = typename base::Tdata;
    using Tsize = typename base::Tsize;

    static const Tsize $capicity = Icapicity;

#pragma region constructor
    List() noexcept {
        base::data_     = reinterpret_cast<T*>(buff_);
        base::capacity_ = $capicity;
    }

    template<class ...U>
    explicit List(Tsize count, U&& ...us) : List{} {
        appends(count, fwd<U>(us)...);
    }

    template<class U, class = $when_is<U, Tdata> >
    List(const View<U>& view) : List{} {
        base::operator+=(view);
    }

    template<class U, class = $when_is<U, Tdata> >
    List(const View<const U>& view) : List{} {
        base::operator+=(view);
    }

    template<u32 SN >
    List(const Tdata(&view)[SN]) : List{} {
        base::operator+=(view);
    }

    ~List()
    {}

    List(List&& rhs) noexcept : List{} {
        *this = static_cast<List&&>(rhs);
    }

    List(const List& rhs): List{} {
        *this = rhs;
    }

    List& operator=(List&& rhs) {
        base::operator=(static_cast<base&&>(rhs));
        return *this;
    }

    List& operator=(const List& rhs) {
        base::operator=(rhs);
        return *this;
    }
#pragma endregion

#pragma region save/load
    static List load(const io::File& file) {
        List list;
        base::loadFile(list, file);
        return list;
    }
#pragma endregion

protected:
    struct alignas(Tdata) {
        u8 data_[sizeof(Tdata)];
    } buff_[$capicity];
};

}

