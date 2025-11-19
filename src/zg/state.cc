/*
 * This file is a part of the TChecker project.
 *
 * See files AUTHORS and LICENSE for copyright details.
 *
 */

#if BOOST_VERSION <= 106600
#include <boost/functional/hash.hpp>
#else
#include <boost/container_hash/hash.hpp>
#endif

#include "tchecker/zg/state.hh"

namespace tchecker {

namespace zg {

state_t::state_t(tchecker::intrusive_shared_ptr_t<tchecker::shared_vloc_t> const & vloc,
                 tchecker::intrusive_shared_ptr_t<tchecker::shared_intval_t> const & intval,
                 tchecker::intrusive_shared_ptr_t<tchecker::zg::shared_zone_t> const & zone)
    : tchecker::ta::state_t(vloc, intval), _zone(zone)
{
  assert(_zone.ptr() != nullptr);
}

state_t::state_t(tchecker::ta::state_t const & s, tchecker::intrusive_shared_ptr_t<tchecker::shared_vloc_t> const & vloc,
                 tchecker::intrusive_shared_ptr_t<tchecker::shared_intval_t> const & intval,
                 tchecker::intrusive_shared_ptr_t<tchecker::zg::shared_zone_t> const & zone)
    : tchecker::ta::state_t(s, vloc, intval), _zone(zone)
{
  assert(_zone.ptr() != nullptr);
}

bool operator==(tchecker::zg::state_t const & s1, tchecker::zg::state_t const & s2)
{
  return tchecker::ta::operator==(s1, s2) && (s1.zone() == s2.zone());
}

bool operator!=(tchecker::zg::state_t const & s1, tchecker::zg::state_t const & s2) { return !(s1 == s2); }

bool shared_equal_to(tchecker::zg::state_t const & s1, tchecker::zg::state_t const & s2)
{
  return tchecker::ta::shared_equal_to(s1, s2) && (s1.zone_ptr() == s2.zone_ptr());
}

bool operator<=(tchecker::zg::state_t const & s1, tchecker::zg::state_t const & s2)
{
  return tchecker::ta::operator==(s1, s2) && (s1.zone() <= s2.zone());
}

bool shared_is_le(tchecker::zg::state_t const & s1, tchecker::zg::state_t const & s2)
{
  // 参数：s1、s2 分别为共享内部结构的两个 zone 图状态；返回值：若 s1 在仿真序下被 s2 覆盖则为 true。
  // 机制：比较离散骨架指针确保两者指向同一控制点，再判断 zone 指针是否复用或通过 zone<= 进行包含检测。
  // 输出语义：true 表示 s1 可由 s2 模拟，即所有后继行为都包含在 s2 中；false 表示需要保留 s1。
  // 共享组件上的仿真判定：离散骨架一致且 zone 指针相同或出现 zone 包含
  
  return tchecker::ta::shared_equal_to(s1, s2) && (s1.zone_ptr() == s2.zone_ptr() || s1.zone() <= s2.zone());
}

bool is_alu_le(tchecker::zg::state_t const & s1, tchecker::zg::state_t const & s2, tchecker::clockbounds::map_t const & l,
               tchecker::clockbounds::map_t const & u)
{
  return tchecker::ta::operator==(s1, s2) && s1.zone().is_alu_le(s2.zone(), l, u);
}

bool shared_is_alu_le(tchecker::zg::state_t const & s1, tchecker::zg::state_t const & s2,
                      tchecker::clockbounds::map_t const & l, tchecker::clockbounds::map_t const & u)
{
  return tchecker::ta::shared_equal_to(s1, s2) && (s1.zone_ptr() == s2.zone_ptr() || s1.zone().is_alu_le(s2.zone(), l, u));
}

std::size_t hash_value(tchecker::zg::state_t const & s)
{
  std::size_t h = tchecker::ta::hash_value(s);
  boost::hash_combine(h, s.zone());
  return h;
}

std::size_t shared_hash_value(tchecker::zg::state_t const & s)
{
  std::size_t h = tchecker::ta::shared_hash_value(s);
  boost::hash_combine(h, s.zone_ptr());
  return h;
}

int lexical_cmp(tchecker::zg::state_t const & s1, tchecker::zg::state_t const & s2)
{
  int ta_cmp = tchecker::ta::lexical_cmp(s1, s2);
  if (ta_cmp != 0)
    return ta_cmp;
  return s1.zone().lexical_cmp(s2.zone());
}

} // end of namespace zg

} // end of namespace tchecker
