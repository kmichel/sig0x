#ifndef INCLUDED_SIG0X
#define INCLUDED_SIG0X

#include <memory>

namespace sig0x {

template <typename... Params>
class Slot {
    struct Socket {
        Socket* prev;
        Socket* next;
        unsigned int refs;
        explicit Socket(Socket* prev_, Socket* next_)
            : prev(prev_), next(next_), refs(1) {
            prev_->next = this;
            next_->prev = this;
        }
        virtual ~Socket() {}
        virtual void operator()(Params... params) {
            (*next)(params...);
        }
        void release() {
            if (prev != 0) {
                prev->next = next;
                next->prev = prev;
                prev = 0;
                refs--;
            }
            decref();
        }
        void incref() {
            refs++;
        }
        void decref() {
            if (refs == 0) {
                delete this;
            } else {
                refs--;
            }
        }
        Socket(const Socket&) = delete;
        Socket& operator=(const Socket&) = delete;
    };

    public:
    class Link {
        Socket* socket;
        public:
        Link()
            : socket(0) {}
        explicit Link(Socket* socket_)
            : socket(socket_) {}
        Link(const Link& that)
            : socket(that.socket) {
            if (socket) socket->incref();
        }
        Link(Link&& that)
            : socket(that.socket) {
            that.socket = 0;
        }
        Link& operator=(const Link& that) {
            if (that.socket) that.socket->incref();
            if (socket) socket->decref();
            socket = that.socket;
            return *this;
        }
        Link& operator=(Link&& that) {
            std::swap(socket, that.socket);
            return *this;
        }

        void release() {
            if (socket) {
                socket->release();
                socket = 0;
            }
        }

        ~Link() {
            if (socket) socket->decref();
        }
    };
    struct AutoLink {
        Link connection;
        AutoLink()
            : connection(0) {}
        explicit AutoLink(const Link& connection_)
            : connection(connection_) {}
        explicit AutoLink(Link&& connection_)
            : connection(std::move(connection_)) {}
        AutoLink& operator=(const Link& connection_) {
            connection = connection_;
            return *this;
        }
        AutoLink& operator=(Link&& connection_) {
            connection = std::move(connection_);
            return *this;
        }
        ~AutoLink() {
            connection.release();
        }
    };
    private:

    struct NullSocket : Socket {
        NullSocket() : Socket(this, this) {}
        virtual void operator()(Params...) {}
    };

    template <typename Functor>
    struct FunctorSocket : Socket {
        Functor functor;
        FunctorSocket(Socket* prev_, Socket* next_, Functor&& functor_)
            : Socket(prev_, next_)
            , functor(std::move(functor_)) {}
        virtual void operator()(Params... params) {
            functor(params...);
            Socket::operator()(params...);
        }
    };

    template <void (*Func)(Params...)>
    struct StaticFuncSocket : Socket {
        StaticFuncSocket(Socket* prev_, Socket* next_)
            : Socket(prev_, next_) {}
        virtual void operator()(Params... params) {
            Func(params...);
            Socket::operator()(params...);
        }
    };

    template <typename This>
    struct MethSocket : Socket {
        This& zis;
        void (This::*func)(Params...);
        MethSocket(Socket* prev_, Socket* next_,
                This& zis_, void (This::*func_)(Params...))
            : Socket(prev_, next_)
            , zis(zis_)
            , func(func_) {}
        virtual void operator()(Params... params) {
            (zis.*func)(params...);
            Socket::operator()(params...);
        }
    };

    template <typename This, void (This::*Func)(Params...)>
    struct StaticMethSocket : Socket {
        This& zis;
        StaticMethSocket(Socket* prev_, Socket* next_, This& zis_)
            : Socket(prev_, next_)
            , zis(zis_) {}
        virtual void operator()(Params... params) {
            (zis.*Func)(params...);
            Socket::operator()(params...);
        }
    };

    NullSocket sockets;
    public:
    Slot() : sockets() {}
    ~Slot() {
        while (sockets.next != &sockets) {
            Socket* next = sockets.next->next;
            sockets.next->prev = 0;
            sockets.next->decref();
            sockets.next = next;
        }
    }

    template <typename Functor> struct CheckCall {
        static auto checker(Functor&& reciever, Params... params)
            -> decltype(reciever(params...));
    };

    template <typename Functor,
        typename = decltype(&CheckCall<Functor>::checker) >
    Link bind(
        Functor&& functor) {
        return Link(
            new FunctorSocket<Functor>(sockets.prev, &sockets, std::move(functor)));
    }

    template <void (*Func)(Params...)>
    Link bind() {
        return Link(
            new StaticFuncSocket<Func>(sockets.prev, &sockets));
    }

    template <typename This>
    Link bind(This& zis, void (This::*func)(Params...)) {
        return Link(
            new MethSocket<This>(sockets.prev, &sockets, zis, func));
    }

    template <typename This, void (This::*Func)(Params...)>
    Link bind(This& zis) {
        return Link(
            new StaticMethSocket<This, Func>(sockets.prev, &sockets, zis));
    }

    template <typename This>
    struct With {
        Slot& slot;
        This& zis;
        template <void (This::*Func)(Params...)>
        Link bind() {
            return Link(
                new StaticMethSocket<This, Func>(slot.sockets.prev, &slot.sockets, zis));
        }
        Link bind(void (This::*func)(Params...)) {
            return Link(
                new MethSocket<This>(slot.sockets.prev, &slot.sockets, zis, func));
        }
    };
    template <typename This>
    With<This> with(This& zis) {
        return {*this, zis};
    }
    template <typename This>
    With<This> with(This* zis) {
        return {*this, *zis};
    }

    void fire(Params... params) {
        (*sockets.next)(params...);
    }
    void operator()(Params... params) {
        fire(params...);
    }

    Slot(const Slot&) = delete;
    Slot& operator=(const Slot&) = delete;
};

}

#endif
