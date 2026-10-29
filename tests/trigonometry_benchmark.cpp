#include "render/continuous_pose.h"
#include "legacy_trigonometry.h"
#include "vectorstorm/sincos.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <numbers>
#include <string_view>
#include <vector>

namespace {
constexpr double pi=std::numbers::pi,tau=2*pi;
struct pair { double sine,cosine; };
struct standard {
  pair operator()(double x) const { return {std::sin(x),std::cos(x)}; }
};
struct vectorstorm_pair {
  pair operator()(double x) const {
#if (defined(__GNUC__) && !defined(__clang__)) || defined(__EMSCRIPTEN__)
    pair result;sincos_any(x,result.sine,result.cosine);return result;
#else
    return standard{}(x); // Vectorstorm's generic double wrapper needs GCC's builtin.
#endif
 }
};
struct single_precision {
  pair operator()(double x) const { float a=static_cast<float>(x);return {std::sin(a),std::cos(a)}; }
};
struct original {
  pair operator()(double x) const {
    auto const phase=static_cast<unsigned>(static_cast<int>(std::floor(x*1024/tau)))&1023;
    auto const &values=tfx::test_reference::sine_table();
    return {values[phase]/32768.0,values[(phase+256)&1023]/32768.0};
  }
};
template<size_t N,bool Interpolate=true> struct lookup {
  std::array<double,N> values{};
  lookup() { for(size_t i=0;i<N;++i)values[i]=std::sin(static_cast<double>(i)*tau/N); }
  pair operator()(double x) const {
    // All measured display angles are in [-pi,pi]; also accept any finite
    // angle here so range reduction is not hidden from the implementation.
    if(x < -pi || x > pi)x=std::remainder(x,tau);
    double phase=x*(N/tau);if(phase<0)phase+=N;
    auto i=static_cast<size_t>(phase);double fraction=phase-static_cast<double>(i);i&=N-1;
    auto read=[&](size_t index){
      if constexpr(Interpolate)return values[index]+fraction*(values[(index+1)&(N-1)]-values[index]);
      else return values[index];
    };
    return {read(i),read((i+N/4)&(N-1))};
  }
};
struct atan_standard { double operator()(double y,double x)const{return std::atan2(y,x);} };
struct atan_original { double operator()(double y,double x)const{return tfx::render::angle_radians(tfx::test_reference::direction_angle(static_cast<int16_t>(y*32767),static_cast<int16_t>(x*32767)));} };
template<size_t N> struct atan_lookup {
  std::array<double,N+1> values{};
  atan_lookup(){for(size_t i=0;i<=N;++i)values[i]=std::atan(static_cast<double>(i)/N);}
  double operator()(double y,double x)const{
    double ax=std::abs(x),ay=std::abs(y),high=std::max(ax,ay),low=std::min(ax,ay);
    if(!high)return std::atan2(y,x);
    double phase=low/high*N;size_t i=std::min(N-1,static_cast<size_t>(phase));
    double angle=values[i]+(phase-static_cast<double>(i))*(values[i+1]-values[i]);
    if(ay>ax)angle=pi/2-angle;
    if(std::signbit(x))angle=pi-angle;
    return std::copysign(angle,y);
  }
};
// Volatile observation is outside the measured loop. No volatile stores in
// each evaluation, no constant inputs and no fast-math compiler flags.
volatile double observed;
template<class F> double measure(F const &fn) {
  std::array<double,7> samples{};
  for(auto &sample:samples){auto start=std::chrono::steady_clock::now();double sum=fn();auto end=std::chrono::steady_clock::now();observed=sum;sample=std::chrono::duration<double,std::nano>(end-start).count();}
  std::ranges::sort(samples);return samples[3];
}
template<class F> void bench(std::string_view name,F const &fn,std::vector<double> const &angles){
  double error=0,norm_error=0;
  for(size_t i=0;i<=131072;++i){double a=-pi+tau*static_cast<double>(i)/131072;auto v=fn(a);error=std::max({error,std::abs(v.sine-std::sin(a)),std::abs(v.cosine-std::cos(a))});norm_error=std::max(norm_error,std::abs(v.sine*v.sine+v.cosine*v.cosine-1));}
  auto const pairs=measure([&]{double sum=0;for(unsigned pass=0;pass<8;++pass)for(double a:angles){auto v=fn(a);sum+=v.sine+v.cosine*.7;}return sum;})/static_cast<double>(8*angles.size());
  auto const matrices=measure([&]{double sum=0;for(unsigned pass=0;pass<8;++pass)for(size_t i=0;i+2<angles.size();i+=3){auto p=fn(angles[i]),h=fn(angles[i+1]),r=fn(angles[i+2]);sum+=h.cosine*r.cosine+h.sine*p.sine*r.sine-p.cosine*r.sine-h.sine*r.cosine+h.cosine*p.sine*r.sine+h.cosine*r.sine-h.sine*p.sine*r.cosine+p.cosine*r.cosine-h.sine*r.sine-h.cosine*p.sine*r.cosine+h.sine*p.cosine+p.sine+h.cosine*p.cosine;}return sum;})/static_cast<double>(8*(angles.size()/3));
  std::cout<<name<<','<<pairs<<','<<matrices<<','<<error<<','<<norm_error<<'\n';
}
template<class F> void bench_atan(std::string_view name,F const &fn,std::vector<double> const &angles){
  std::vector<pair> points;for(double a:angles)points.push_back(standard{}(a));
  double error=0;for(auto v:points)error=std::max(error,std::abs(std::remainder(fn(v.sine,v.cosine)-std::atan2(v.sine,v.cosine),tau)));
  auto const ns=measure([&]{double sum=0;for(unsigned pass=0;pass<8;++pass)for(auto v:points)sum+=fn(v.sine,v.cosine);return sum;})/static_cast<double>(8*points.size());
  std::cout<<name<<','<<ns<<','<<error<<','<<error*180/pi<<'\n';
}
}
int main(){
  std::vector<double> angles;uint32_t rng=0x12345678;
  for(unsigned i=0;i<65536;++i){rng=rng*1664525+1013904223;angles.push_back((rng/4294967296.0-.5)*tau);}
  std::cout<<std::setprecision(9)<<"sincos,ns_per_pair,ns_per_rotation,max_absolute_error,max_unit_circle_error\n";
  bench("native_1024",original{},angles);bench("std_double",standard{},angles);
#if (defined(__GNUC__) && !defined(__clang__)) || defined(__EMSCRIPTEN__)
  bench("vectorstorm_pair",vectorstorm_pair{},angles);
#else
  bench("vectorstorm_std_fallback",vectorstorm_pair{},angles);
#endif
  bench("std_float",single_precision{},angles);
  bench("table_65536_floor",lookup<65536,false>{},angles);bench("linear_4096",lookup<4096>{},angles);bench("linear_16384",lookup<16384>{},angles);
  std::cout<<"atan2,ns_per_call,max_error_radians,max_error_degrees\n";
  bench_atan("native_phase",atan_original{},angles);bench_atan("std_atan2",atan_standard{},angles);bench_atan("linear_atan_4096",atan_lookup<4096>{},angles);
}
