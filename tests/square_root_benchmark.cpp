#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>
#include "math/native.h"
#include "vectorstorm/sqrt_fast.h"

namespace {
struct standard {
  template<class T> T root(T x) const { return std::sqrt(x); }
  template<class T> T inverse(T x) const { return T{1}/std::sqrt(x); }
};
struct vectorstorm_fast {
  template<class T> T root(T x) const { return sqrt_fast(x); }
  template<class T> T inverse(T x) const { return sqrt_inv_fast(x); }
};
struct vectorstorm_coarse {
  template<class T> T root(T x) const { return sqrt_coarse(x); }
  template<class T> T inverse(T x) const { return sqrt_inv_coarse(x); }
};
#ifdef __SSE__
struct vectorstorm_sse {
  float root(float x) const { return sqrt_sse(x); }
  float inverse(float x) const { return sqrt_inv_sse(x); }
};
// A measured alternative, not a function provided by Vectorstorm. Use its
// single-precision SSE seed, then refine using the caller's precision.
struct sse_refined {
  template<class T> bool supported(T x) const {
    return x>=static_cast<T>(std::numeric_limits<float>::min()) && x<=static_cast<T>(std::numeric_limits<float>::max());
  }
  template<class T> T inverse(T x) const {
    if(!supported(x))return standard{}.inverse(x);
    T y{sqrt_inv_sse(static_cast<float>(x))};
    return y*(T{1.5}-T{.5}*x*y*y);
  }
  template<class T> T root(T x) const { return supported(x) ? x*inverse(x) : std::sqrt(x); }
};
#endif
volatile double observed;
template<class F> double measure(F const &function) {
  std::array<double,9> timings;
  for(auto &time:timings) {
    auto const start=std::chrono::steady_clock::now();
    double const checksum=function();
    auto const end=std::chrono::steady_clock::now();
    observed=checksum;
    time=std::chrono::duration<double,std::nano>(end-start).count();
  }
  std::ranges::sort(timings);
  return timings[timings.size()/2];
}
uint32_t seed=0x73515254;
double random_unit() { seed=seed*1664525+1013904223;return seed/4294967296.0; }
template<class T> std::vector<T> inputs() {
  std::vector<T> values;values.reserve(65536);
  // Positive, normal, finite values covering pixel distances, unit vectors,
  // Q15 axes and world-space squared distances. Accuracy also tests edges.
  for(unsigned i=0;i<65536;++i) values.push_back(static_cast<T>(std::exp2(-24+72*random_unit())));
  return values;
}
template<class T,class F> double throughput(std::vector<T> const &values,F const &fn) {
  return measure([&] {
    std::array<double,4> sums{};
    for(unsigned pass=0;pass<8;++pass) for(size_t i=0;i<values.size();i+=4) {
      for(size_t lane=0;lane<4;++lane)sums[lane]+=fn(values[i+lane]);
    }
    return sums[0]+sums[1]+sums[2]+sums[3];
  })/static_cast<double>(8*values.size());
}
template<class T,class F> double latency(std::vector<T> const &values,F const &fn) {
  return measure([&] {
    T previous{1};
    for(unsigned pass=0;pass<4;++pass) for(T value:values)previous=fn(value+previous*T{.25});
    return static_cast<double>(previous);
  })/static_cast<double>(4*values.size());
}
template<class T,class Backend> void scalar(std::string_view name,Backend backend,std::vector<T> const &values) {
  auto root=[&](T x){return backend.root(x);};
  auto inverse=[&](T x){return backend.inverse(x);};
  double root_error=0,inverse_error=0;
  auto accuracy=[&](T x) {
    double const expected=std::sqrt(static_cast<double>(x));
    if(!std::isfinite(root(x)) || !std::isfinite(inverse(x)) || root(x)<=0 || inverse(x)<=0)
      throw std::runtime_error{std::string{name}+" failed on positive normal input"};
    root_error=std::max(root_error,std::abs(static_cast<double>(root(x))/expected-1));
    inverse_error=std::max(inverse_error,std::abs(static_cast<double>(inverse(x))*expected-1));
  };
  for(T x:values)accuracy(x);
  // Dense mantissa sweep over both exponent parities: these methods repeat
  // their relative error pattern every power of four for normal inputs.
  for(unsigned i=0;i<=262144;++i)accuracy(static_cast<T>(1+3.0*i/262144));
  std::cout<<(std::is_same_v<T,float> ? "float" : "double")<<','<<name<<','
    <<throughput(values,root)<<','<<latency(values,root)<<','
    <<throughput(values,inverse)<<','<<latency(values,inverse)<<','
    <<root_error<<','<<inverse_error<<'\n';
}
template<class T,class F> void normalise(std::string_view name,std::vector<vec3<T>> const &values,F const &fn) {
  double length_error=0;
  for(auto const &v:values) {
    auto const n=fn(v);
    length_error=std::max(length_error,std::abs(std::hypot(static_cast<double>(n.x),static_cast<double>(n.y),static_cast<double>(n.z))-1));
  }
  double const ns=measure([&] {
    std::array<double,4> sums{};
    for(unsigned pass=0;pass<8;++pass) for(size_t i=0;i<values.size();++i) {
      auto const n=fn(values[i]);sums[i&3]+=n.x+n.y*.7+n.z*.3;
    }
    return sums[0]+sums[1]+sums[2]+sums[3];
  })/static_cast<double>(8*values.size());
  std::cout<<(std::is_same_v<T,float> ? "float" : "double")<<','<<name<<','<<ns<<','<<length_error<<'\n';
}
template<class T> void normals() {
  std::vector<vec3<T>> values;values.reserve(65536);
  for(unsigned i=0;i<65536;++i) {
    double const magnitude=std::exp2(-8+24*random_unit());
    values.emplace_back(static_cast<T>((random_unit()-.5)*magnitude),static_cast<T>((random_unit()-.5)*magnitude),static_cast<T>((random_unit()-.5)*magnitude));
  }
  normalise<T>("vectorstorm_std",values,[](auto const &v){return v.template normalise_copy<sqrt_mode::std>();});
  normalise<T>("vectorstorm_fast",values,[](auto const &v){return v.template normalise_copy<sqrt_mode::fast>();});
  normalise<T>("vectorstorm_coarse",values,[](auto const &v){return v.template normalise_copy<sqrt_mode::coarse>();});
  normalise<T>("standard_reciprocal_multiply",values,[](auto const &v){return v*standard{}.inverse(v.dot(v));});
  normalise<T>("fast_reciprocal_multiply",values,[](auto const &v){return v*vectorstorm_fast{}.inverse(v.dot(v));});
  normalise<T>("coarse_reciprocal_multiply",values,[](auto const &v){return v*vectorstorm_coarse{}.inverse(v.dot(v));});
#ifdef __SSE__
  if constexpr(std::is_same_v<T,float>)normalise<T>("sse_reciprocal_multiply",values,[](auto const &v){return v*vectorstorm_sse{}.inverse(v.dot(v));});
  normalise<T>("sse_refined_guarded_reciprocal_multiply",values,[](auto const &v){return v*sse_refined{}.inverse(v.dot(v));});
#endif
}
template<class T,class Backend> void edges(std::string_view name,Backend const &backend) {
  std::array<std::pair<std::string_view,T>,9> const cases{{
    {"zero",0},{"negative_zero",-T{0}},{"min_subnormal",std::numeric_limits<T>::denorm_min()},
    {"min_normal",std::numeric_limits<T>::min()},{"one",1},{"max_finite",std::numeric_limits<T>::max()},
    {"infinity",std::numeric_limits<T>::infinity()},{"negative",-1},{"nan",std::numeric_limits<T>::quiet_NaN()},
  }};
  // Read each value through volatile so constants do not bypass the exact
  // compiled approximation used on runtime data.
  for(auto const &[label,input]:cases) {
    volatile T runtime=input;T const x=runtime;
    std::cout<<(std::is_same_v<T,float> ? "float" : "double")<<','<<name<<','<<label<<','<<backend.root(x)<<','<<backend.inverse(x)<<'\n';
  }
}
}
int main() {
  std::cout<<std::setprecision(10);
  auto const floats=inputs<float>();auto const doubles=inputs<double>();
  std::cout<<"precision,scalar_method,root_throughput_ns,root_latency_ns,inverse_throughput_ns,inverse_latency_ns,max_root_relative_error,max_inverse_relative_error\n";
  scalar("std",standard{},floats);scalar("fast",vectorstorm_fast{},floats);scalar("coarse",vectorstorm_coarse{},floats);
#ifdef __SSE__
  scalar("sse",vectorstorm_sse{},floats);scalar("sse_refined_guarded",sse_refined{},floats);
#endif
  scalar("std",standard{},doubles);scalar("fast",vectorstorm_fast{},doubles);scalar("coarse",vectorstorm_coarse{},doubles);
#ifdef __SSE__
  scalar("sse_refined_float_seed_guarded",sse_refined{},doubles);
#endif
  std::cout<<"precision,normalisation_method,ns_per_vector,max_unit_length_error\n";
  normals<float>();normals<double>();
  std::cout<<"precision,edge_method,input,root,inverse\n";
  edges<float>("std",standard{});edges<float>("fast",vectorstorm_fast{});edges<float>("coarse",vectorstorm_coarse{});
#ifdef __SSE__
  edges<float>("sse",vectorstorm_sse{});edges<float>("sse_refined_guarded",sse_refined{});
#endif
  edges<double>("std",standard{});edges<double>("fast",vectorstorm_fast{});edges<double>("coarse",vectorstorm_coarse{});
#ifdef __SSE__
  edges<double>("sse_refined_float_seed_guarded",sse_refined{});
#endif
}
