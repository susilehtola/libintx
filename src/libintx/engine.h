#ifndef LIBINTX_ENGINE_H
#define LIBINTX_ENGINE_H

#include "libintx/array.h"
#include "libintx/shell.h"
#include "libintx/config.h"

#include <vector>
#include <string>
#include <utility>
#include <memory>
#include <stdexcept>
#include <functional>

namespace libintx {

  struct parameters_exceed_max_am : std::domain_error {
    parameters_exceed_max_am(int AB, int X) :
      std::domain_error(
        "Parameters"
        " AB=" + std::to_string(AB) +
        " X=" + std::to_string(X) +
        "exceed "
        "LIBINTX_MAX_L=" + std::to_string(LMAX) + " "
        "LIBINTX_MAX_X=" + std::to_string(XMAX)
      )
    {
    }
  };

  template<int>
  struct Kernel;

  template<>
  struct Kernel<3> {
    virtual ~Kernel() = default;
    virtual const double* compute(
      const Double<3>&, const Double<3>&,
      const Double<3>&
    ) = 0;
    virtual const double* buffer() = 0;
  };

  template<>
  struct Kernel<4> {
    virtual ~Kernel() = default;
    virtual const double* compute(
      const Double<3>&, const Double<3>&,
      const Double<3>&, const Double<3>&
    ) = 0;
  };

  template<int Order, class ... Args>
  struct Engine;

  template<>
  struct Engine<3> {

    using IntegralList = std::vector< std::tuple<Index<3>, double*> >;

    Engine(
      const std::vector< std::tuple<Gaussian,Double<3> > > &,
      const std::vector< std::tuple<Gaussian,Double<3> > > &
    );

    void compute(const IntegralList &list);

  protected:
    std::vector< std::tuple<Gaussian,Double<3> > > basis_;
    std::vector< std::tuple<Gaussian,Double<3> > > df_basis_;

  };

  template<class Kernel, class F, size_t ... ABs, size_t ... Xs>
  static Kernel make_ab_x_kernel(
    F f, size_t AB, size_t X,
    std::index_sequence<ABs...>,
    std::index_sequence<Xs...>)
  {
    using Factory = std::function<Kernel()>;
    auto kernel_table = make_array<Factory>(
      [f](auto AB, auto X) {
        return Factory(
          [f,AB,X](){ return f(AB,X); }
        );
      },
      std::index_sequence<ABs...>{},
      std::index_sequence<Xs...>{}
    );
    if (AB < sizeof...(ABs) && X < sizeof...(Xs)) {
      return kernel_table[AB][X]();
    }
    throw parameters_exceed_max_am(AB,X);
  }

  template<class Kernel, class F>
  auto make_ab_x_kernel(F f, size_t AB, size_t X) {
    auto ABs = std::make_index_sequence<LMAX*2+1>{};
    auto Xs = std::make_index_sequence<XMAX+1>{};
    auto kernel = make_ab_x_kernel<Kernel>(f, AB, X, ABs, Xs);
    assert(kernel);
    if (!kernel) {
      throw parameters_exceed_max_am(AB,X);
    }
    return kernel;
  }

}

#endif /* LIBINTX_ENGINE_H */
