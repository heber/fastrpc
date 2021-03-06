#pragma once

#include "rpc_common/compiler.hh"
#include "libev_loop.hh"
#include "async_rpcc.hh"
#include "proto/fastrpc_proto.hh"

namespace rpc {

/** async_batched_rpcc promises that:
 *   - the callback of every request will be called once, even on failure.
 *   - on failure, the connection will first be disconnected, then all
 *     the outstanding requests will be called to complete with its eno
 *     set to RPCERR.
 */
template <typename T>
class async_batched_rpcc : public rpc_handler<T>, public async_rpcc<T> {
  public:
    async_batched_rpcc(const char* rmt, int rmtport, int w,
		       const char* local = "0.0.0.0", bool force_connected = true)
	: async_rpcc<T>(new multi_tcpp(rmt, local, rmtport), this, force_connected, NULL), 
          loop_(nn_loop::get_tls_loop()), w_(w) {
    }
    bool drain() {
        mandatory_assert(loop_->enter() == 1,
                         "Don't call drain within a libev_loop!");
        bool work_done = this->noutstanding();
        while (this->noutstanding())
            loop_->run_once();
        loop_->leave();
        return work_done;
    }
    void handle_rpc(async_rpcc<T>*, parser&) {
	mandatory_assert(0 && "rpc client can't process rpc requests");
    }
    // called before outstanding requests are completed with error
    void handle_client_failure(async_rpcc<T>* c) {
	mandatory_assert(c == static_cast<async_rpcc<T>*>(this));
    }
    void handle_post_failure(async_rpcc<T>* c) {
	mandatory_assert(c == static_cast<async_rpcc<T>*>(this));
    }
    template <uint32_t PROC>
    inline void call(gcrequest_iface<PROC> *q) {
	this->buffered_call(q);
	winctrl();
    }

  protected:
    void winctrl() {
	assert(w_ > 0);
	if (!this->connected())
	    return;
        if (w_ == 1 || this->noutstanding() % (w_/2) == 0)
            this->flush();
        if (loop_->enter() == 1) {
            while (this->noutstanding() >= w_)
                loop_->run_once();
        }
        loop_->leave();
    }
  private:
    nn_loop *loop_;
    int w_;
};

template <typename T>
class make_reply_helper {
  public:
    make_reply_helper(T &x)
	: x_(x) {
    }
    template <typename U>
    void operator()(const U &, const T &x) {
	x_ = x;
    }
  private:
    T &x_;
};

template <typename T> make_reply_helper<T> make_reply(T &x) {
    x.set_eno(app_param::ErrorCode::UNKNOWN);
    return make_reply_helper<T>(x);
}

#if 0
template <typename T, typename REQ, typename REPLY,
	  void (T::*method)(REPLY &)>
class make_unary_call_helper {
  public:
    make_unary_call_helper(T *obj) : obj_(obj) {
    }
    void operator()(REQ &, REPLY &x) {
	(obj_->*method)(x);
    }
  private:
    T *obj_;
};

template <typename T, typename REQ, typename REPLY,
	  void (T::*method)(REQ &, REPLY &)>
class make_binary_call_helper {
  public:
    make_binary_call_helper(T *obj) : obj_(obj) {
    }
    void operator()(REQ &x, REPLY &y) {
	(obj_->*method)(x, y);
    }
  private:
    T *obj_;
};
#endif

} // namespace rpc

