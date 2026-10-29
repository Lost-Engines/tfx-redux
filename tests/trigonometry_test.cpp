#include "math/attitude.h"
#include "render/continuous_pose.h"
#include "sensors/ground.h"
#include "flight/aerodynamic_rotation_internal.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <numbers>
#include <stdexcept>

namespace {
void require(bool ok,char const *message) { if(!ok)throw std::runtime_error{message}; }
constexpr long double tau=2*std::numbers::pi_v<long double>;
long double radians(unsigned angle) { return angle*tau/65536; }
}
int main() {
  using namespace tfx;
  long double max_error=0,max_bearing_error=0;
  for(unsigned units=0;units<65536;++units) {
    auto const angle=static_cast<math::angle>(units);
    long double const s=std::sin(radians(units)),c=std::cos(radians(units));
    max_error=std::max({max_error,std::abs(math::sine_q15(angle)-32767*s),std::abs(math::cosine_q15(angle)-32767*c)});
    require(math::sine_q15(angle)==-math::sine_q15(-static_cast<int32_t>(units)),"sine is not antisymmetric");
    require(math::cosine_q15(angle)==math::cosine_q15(-static_cast<int32_t>(units)),"cosine is not symmetric");
    require(std::abs(math::sine_q15(angle)-math::cosine_q15(static_cast<int32_t>(units)-16384))<=1,"sine/cosine quadrature differs");
    // Large input vectors ensure the bearing test covers all 65,536 output
    // angles, independently of a small signed-word input's quantisation.
    int32_t const y=static_cast<int32_t>(std::llround(s*100000000)),x=static_cast<int32_t>(std::llround(c*100000000));
    auto const bearing=math::direction_angle(y,x);
    auto const difference=math::signed_word(static_cast<uint16_t>(bearing-angle));
    max_bearing_error=std::max(max_bearing_error,std::abs(static_cast<long double>(difference)));
    require(std::abs(difference)<=1,"atan2 loses angle precision or changes quadrants");
    auto const rotated=math::rotate_words({30000,-10000},angle);
    auto wrap=[](long double value) { return math::signed_word(static_cast<uint16_t>(static_cast<int64_t>(std::floor(value)))); };
    require(std::abs(static_cast<int>(math::signed_word(static_cast<uint16_t>(rotated.x-wrap((30000*c+10000*s)*32767/32768)))))<=3,"2D rotation x differs from independent geometry");
    require(std::abs(static_cast<int>(math::signed_word(static_cast<uint16_t>(rotated.y-wrap((30000*s-10000*c)*32767/32768)))))<=3,"2D rotation y differs from independent geometry");
  }
  require(max_error<1.000001L,"fixed-point trig exceeds one amplitude unit");
  require(math::sine_q15(16384)==32767 && math::sine_q15(49152)==-32767,"cardinal sine retains the original asymmetric peak");
  require(math::direction_angle(0,0)==0 && math::direction_angle(0,-1)==32768,"undefined or reversed axial bearing");
  require(math::direction_angle(std::numeric_limits<int32_t>::min(),std::numeric_limits<int32_t>::min())==40960,"wide negative bearing overflowed");
  // Cartesian Euler geometry checks layout/signs without using the fixed-point
  // sum/difference identities. Include angles between the former table bins.
  for(unsigned p:{1u,17u,8193u,16384u,49153u})for(unsigned h:{1u,63u,17003u,65535u})for(unsigned r:{0u,9u,16385u,32767u}) {
    auto const matrix=math::rotation_words({static_cast<math::angle>(p),static_cast<math::angle>(h),static_cast<math::angle>(r)});
    long double sp=std::sin(radians(p)),cp=std::cos(radians(p)),sh=std::sin(radians(h)),ch=std::cos(radians(h)),sr=std::sin(radians(r)),cr=std::cos(radians(r));
    std::array<long double,9> expected{ch*cr+sh*sp*sr,-cp*sr,-sh*cr+ch*sp*sr,ch*sr-sh*sp*cr,cp*cr,-sh*sr-ch*sp*cr,sh*cp,sp,ch*cp};
    for(unsigned i=0;i<9;++i)require(std::abs(matrix.data[i]-expected[i]*32767)<5,"fixed-point matrix loses precision or axis convention");
  }
  auto const subphase=math::rotation_words({0,1,0});
  require(subphase.data[6]>0 && subphase.data[2]<0,"model rotation still discards low angle bits");
  math::attitude body;
  body.rotate(math::rotation_axis::heading,1);
  require(body.forward.x>0 && body.right.z<0,"flight rotation still discards low angle bits");
  auto const origin=math::orbit_offset_q16({0,0},100);
  require(origin.x==0 && origin.y==0,"orbit retains the old one-table-step offset");
  // Bearing and movement agree on handedness for tiny positive/negative
  // headings. Their stored integer coordinates need not be exact inverses.
  for(int h:{-63,-1,1,63}) {
    vec3<int32_t> position{0,-1000,0};
    math::advance_position(position,{0,static_cast<math::angle>(h),0},20000,-100000,0);
    require(h<0 ? position.x<0 : position.x>0,"movement still stalls inside a table bin");
    auto const bearing=math::bearing({0,-1000,0},position);
    require(std::abs(math::signed_word(static_cast<uint16_t>(bearing.y-h)))<=2,"movement and bearing disagree");
  }
  auto const centre=sensors::ground_intersection({0,-1000,0},8192,0);
  auto const right=sensors::ground_intersection({0,-1000,0},8192,32);
  require(centre.x==0 && right.x>centre.x && std::abs(right.z-centre.z)<2,"ground targeting retains the coarse heading step");
  flight::aerodynamic_motion motion;
  motion.pose.frame.forward={32767,0,0};motion.pose.frame.right={0,32767,0};motion.pose.frame.down={0,0,32767};
  for(unsigned i=0;i<4;++i)flight::rotate_aerodynamic_units(motion,flight::aerodynamic_rotation::roll,.25);
  require(motion.pose.frame.right.z!=0,"military rotation still accumulates 64-unit steps");
  require(std::abs(motion.rotation_remainder.x)<1e-12,"fractional rotation remainder was lost");
  std::cout<<"All 65,536 angles: Q15 error "<<static_cast<double>(max_error)<<", bearing error "<<static_cast<double>(max_bearing_error)<<" angle words; matrix, movement, targeting, orbit and fractional-flight checks passed\n";
}
