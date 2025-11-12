zg-c/*
 * This file is a part of the TChecker project.
 *
 * See files AUTHORS and LICENSE for copyright details.
 *
 */

#include <algorithm>
#include <cassert>
#include <deque>
#include <unordered_map>
#include <unordered_set>

#include <boost/dynamic_bitset.hpp>

#include "counter_example.hh"
#include "tchecker/algorithms/search_order.hh"
#include "tchecker/dbm/dbm.hh"
#include "tchecker/system/static_analysis.hh"
#include "tchecker/ta/ta.hh"
#include "tchecker/ta/state.hh"
#include "tchecker/utils/log.hh"
#include "tchecker/utils/shared_objects.hh"
#include "tchecker/zg/semantics.hh"
#include "zg-covreach.hh"

namespace tchecker {

namespace tck_reach {

namespace zg_covreach {

// 对每个位置做：把“到达该位置后必须保持的约束”逆推回它的前驱，再逆推前驱的前驱……直到全图稳定
class g_simulation_cache_t {
public:
  explicit g_simulation_cache_t(std::shared_ptr<tchecker::zg::zg_t> const & zg)
      : _zg(zg), _dim(zg->system().clocks_count(tchecker::VK_FLATTENED) + 1)
  {
    assert(_zg != nullptr);
  }

  // 为每个新出现的 location 初始化 G(q)。
  void ensure_entry(tchecker::const_vloc_sptr_t const & vloc)
  {
    if (vloc.ptr() == nullptr)
      return;
    if (_entries.find(vloc) != _entries.end())
      return;

    entry e;
    e.dbm.resize(static_cast<std::size_t>(_dim) * static_cast<std::size_t>(_dim));
    tchecker::dbm::universal_positive(e.dbm.data(), _dim);
    for (tchecker::clock_id_t x = 1; x < _dim; ++x)
      e.dbm[static_cast<std::size_t>(x) * _dim + 0] = tchecker::dbm::LE_ZERO;
    tchecker::dbm::tighten(e.dbm.data(), _dim);
    _entries.emplace(vloc, std::move(e));
  }

  bool simulation_leq(tchecker::const_vloc_sptr_t const & vloc, tchecker::zg::zone_t const & lhs,
                      tchecker::zg::zone_t const & rhs) const
  {
    auto it = _entries.find(vloc);
    if (it == _entries.end())
      return lhs <= rhs;

    tchecker::dbm::db_t const * lhs_dbm = lhs.dbm();
    tchecker::dbm::db_t const * rhs_dbm = rhs.dbm();
    auto const & g_dbm = it->second.dbm;

    for (std::size_t i = 0; i < _dim; ++i) {
      for (std::size_t j = 0; j < _dim; ++j) {
        std::size_t idx = i * _dim + j;
        if (g_dbm[idx] == tchecker::dbm::LT_INFINITY)
          continue;
        if (tchecker::dbm::db_cmp(lhs_dbm[idx], rhs_dbm[idx]) > 0)
          return false;
      }
    }
    return true;
  }

  // 记录一条从 q → q′ 的转移，并把约束从 q′ 反推出 q。
  void record_transition(tchecker::zg::state_t const & src_state, tchecker::zg::state_t const & tgt_state,
                         tchecker::zg::transition_t const & transition)
  {
    tchecker::const_vloc_sptr_t const src_vloc =
        tchecker::static_pointer_cast<tchecker::shared_vloc_t const>(src_state.vloc_ptr());
    tchecker::const_vloc_sptr_t const tgt_vloc =
        tchecker::static_pointer_cast<tchecker::shared_vloc_t const>(tgt_state.vloc_ptr());

    ensure_entry(src_vloc);
    ensure_entry(tgt_vloc);

    // 创建一个结构体 g_transition_program_t，收集这条边的所有时钟相关信息
    g_transition_program_t prog;
    prog.src_vloc = src_vloc;
    prog.vedge = transition.vedge_ptr();
    prog.src_invariant = transition.src_invariant_container();
    prog.guard = transition.guard_container();
    prog.reset = transition.reset_container();
    prog.tgt_invariant = transition.tgt_invariant_container();
    prog.src_delay_allowed = tchecker::ta::delay_allowed(_zg->system(), *src_vloc);
    prog.tgt_delay_allowed = tchecker::ta::delay_allowed(_zg->system(), *tgt_vloc);

    // 把这条边放入 _incoming[tgt_vloc] 的桶中
    // _incoming 记录了每个目标位置 q′ 的所有入边
    auto & bucket = _incoming[tgt_vloc];

    // 查找有没有同src和vedge的边
    auto it =
        std::find_if(bucket.begin(), bucket.end(), [&](g_transition_program_t const & existing) {
          return (existing.src_vloc == prog.src_vloc) && (existing.vedge == prog.vedge);
        });

    // 如果没找到（到了最后），是崭新的边，则加入bucket，保证每次转移只记录一份在bucket中
    // 对该新边再次调用 apply_program，把 G(q′) 反推到 G(q)
    if (it == bucket.end()) {
      bucket.push_back(std::move(prog));
      if (apply_program(bucket.back(), tgt_vloc))
        enqueue(bucket.back().src_vloc);
    }
    else {
      // 如果不是新边，则直接调用 apply_program 更新
      // G(tgt_vloc) 可能在先前的某次传播中变得更强/更宽松了
      // 需要把最新的 G(tgt_vloc) 再次通过这条入边向前更新 G(src_vloc)，以继续逼近不动点
      if (apply_program(*it, tgt_vloc)) 
        enqueue(it->src_vloc);
    }

    process_pending();
  }

private:
  struct entry {
    std::vector<tchecker::dbm::db_t> dbm;
  };

  struct g_transition_program_t {
    tchecker::const_vloc_sptr_t src_vloc;
    tchecker::const_vedge_sptr_t vedge;
    tchecker::clock_constraint_container_t src_invariant;
    tchecker::clock_constraint_container_t guard;
    tchecker::clock_reset_container_t reset;
    tchecker::clock_constraint_container_t tgt_invariant;
    bool src_delay_allowed = false;
    bool tgt_delay_allowed = false;
  };

  bool apply_program(g_transition_program_t const & prog, tchecker::const_vloc_sptr_t const & tgt_vloc)
  {
    auto tgt_it = _entries.find(tgt_vloc);
    if (tgt_it == _entries.end())
      return false;

    // 拿到目标位置 q′ 的 G(q′)。复制一份为candidate
    std::vector<tchecker::dbm::db_t> candidate = tgt_it->second.dbm;
    // 相当于论文的 pre(prog,G(q′))，应用目标 invariant 和 guard 的逆向 + 撤销 reset + 考虑源 invariant + 把 delay 的逆向效果处理掉
    // 把目标 q′ 的约束反向传给源 q
    tchecker::state_status_t status =
        _semantics.prev(candidate.data(), _dim, prog.src_delay_allowed, prog.src_invariant, prog.guard, prog.reset,
                        prog.tgt_delay_allowed, prog.tgt_invariant);
    if (status != tchecker::STATE_OK)
      return false;

    // 调 tighten() 保证闭包
    if (tchecker::dbm::tighten(candidate.data(), _dim) == tchecker::dbm::EMPTY)
      return false;

    auto src_it = _entries.find(prog.src_vloc);
    if (src_it == _entries.end())
      return false;

    // 用 widen(target, candidate) 把源位置 q 的 G(q) 更新
    return widen(src_it->second.dbm, candidate);
  }

  bool widen(std::vector<tchecker::dbm::db_t> & target, std::vector<tchecker::dbm::db_t> const & src)
  {
    bool changed = false;
     // 逐元素取 max()，即 G(q)[i,j]:=max(G(q)[i,j], candidate[i,j])，因为 DBM 中数值越大越宽松
    for (std::size_t idx = 0, size = target.size(); idx < size; ++idx) {
      tchecker::dbm::db_t bound = tchecker::dbm::max(target[idx], src[idx]);
      if (bound != target[idx]) {
        target[idx] = bound;
        changed = true;
      }
    }
    // 若有变化，就返回 true，触发继续反向传播
    if (changed)
      tchecker::dbm::tighten(target.data(), _dim);
    return changed;
  }

  void enqueue(tchecker::const_vloc_sptr_t const & vloc)
  {
    if (_in_queue.insert(vloc).second)
      _pending.push_back(vloc);
  }

// 在时间自动机的转移图（每个 location 为节点，edge 为 timed transition）上工作；
// 从某个位置 q′ 出发，查看所有进入它的边 q→q′
// 调 apply_program 把 G(q′) 的约束反推到 G(q)；
// 如果 G(q) 更新（变“紧”或“多”了），就把 q 加入待处理队列；
// 循环，直到所有位置的 G(q) 都稳定（不再更新）
// 等效于 5.1 节公式 (4)
  void process_pending()
  {
    while (!_pending.empty()) {
      tchecker::const_vloc_sptr_t current = _pending.front();
      _pending.pop_front();
      _in_queue.erase(current);

      auto incoming_it = _incoming.find(current);
      if (incoming_it == _incoming.end())
        continue;

      for (g_transition_program_t const & prog : incoming_it->second) {
        if (apply_program(prog, current))
          enqueue(prog.src_vloc);
      }
    }
  }

  std::shared_ptr<tchecker::zg::zg_t> _zg;
  tchecker::clock_id_t _dim;
  tchecker::zg::elapsed_semantics_t _semantics;
  std::unordered_map<tchecker::const_vloc_sptr_t, entry> _entries;
  std::unordered_map<tchecker::const_vloc_sptr_t, std::vector<g_transition_program_t>> _incoming;
  std::deque<tchecker::const_vloc_sptr_t> _pending;
  std::unordered_set<tchecker::const_vloc_sptr_t> _in_queue;
};

/* node_t */

node_t::node_t(tchecker::zg::state_sptr_t const & s, bool initial, bool final)
    : tchecker::graph::node_flags_t(initial, final), tchecker::graph::node_zg_state_t(s)
{
}

node_t::node_t(tchecker::zg::const_state_sptr_t const & s, bool initial, bool final)
    : tchecker::graph::node_flags_t(initial, final), tchecker::graph::node_zg_state_t(s)
{
}

/* node_hash_t */

std::size_t node_hash_t::operator()(tchecker::tck_reach::zg_covreach::node_t const & n) const
{
  // 参数：n 为待插入/查找的图节点；返回值：哈希码。
  // 机制：只读取 timed automaton 的离散分量（位置+整型变量）；哈希一致意味着同一控制点，从而放入同一覆盖桶。
  // 仅使用时序离散部分进行哈希，保证候选覆盖节点拥有相同的位置与整型取值
  // NB: we hash on the discrete (i.e. ta) part of the state in n to check all nodes
  // with same discrete part for covering
  return tchecker::ta::shared_hash_value(n.state());
}

/* node_le_t */

node_le_t::node_le_t(std::shared_ptr<tchecker::tck_reach::zg_covreach::g_simulation_cache_t> const & g_cache)
    : _g_cache(g_cache)
{
}

bool node_le_t::operator()(tchecker::tck_reach::zg_covreach::node_t const & n1,
                           tchecker::tck_reach::zg_covreach::node_t const & n2) const
{
  if (!tchecker::ta::shared_equal_to(n1.state(), n2.state()))
    return false;

  if (_g_cache == nullptr)
    return (n1.state().zone() <= n2.state().zone());

  return _g_cache->simulation_leq(n1.state_ptr()->vloc_ptr(), n1.state().zone(), n2.state().zone());
}

/* edge_t */

edge_t::edge_t(tchecker::zg::transition_t const & t) : tchecker::graph::edge_vedge_t(t.vedge_ptr()) {}

/* graph_t */

graph_t::graph_t(std::shared_ptr<tchecker::zg::zg_t> const & zg,
                 std::shared_ptr<tchecker::tck_reach::zg_covreach::g_simulation_cache_t> const & g_cache,
                 std::size_t block_size, std::size_t table_size)
    : base_graph_t(block_size, table_size, tchecker::tck_reach::zg_covreach::node_hash_t(),
                   tchecker::tck_reach::zg_covreach::node_le_t(g_cache)),
      _g_cache(g_cache), _zg(zg)
{
}

graph_t::edge_sptr_t graph_t::add_edge(graph_t::base_graph_t::node_sptr_t const & src,
                                       graph_t::base_graph_t::node_sptr_t const & tgt,
                                       enum tchecker::graph::subsumption::edge_type_t edge_type, tchecker::zg::transition_t const & t)
{
  auto edge = base_graph_t::add_edge(src, tgt, edge_type, t);
  if (_g_cache != nullptr)
    _g_cache->record_transition(src->state(), tgt->state(), t);
  return edge;
}

bool graph_t::is_actual_edge(edge_sptr_t const & e) const { return edge_type(e) == tchecker::graph::subsumption::EDGE_ACTUAL; }

void graph_t::attributes(tchecker::tck_reach::zg_covreach::node_t const & n, std::map<std::string, std::string> & m) const
{
  _zg->attributes(n.state_ptr(), m);
  tchecker::graph::attributes(static_cast<tchecker::graph::node_flags_t const &>(n), m);
}

void graph_t::attributes(tchecker::tck_reach::zg_covreach::edge_t const & e, std::map<std::string, std::string> & m) const
{
  m["vedge"] = tchecker::to_string(e.vedge(), _zg->system().as_system_system());
}

/* dot_output */

/*!
 \class node_lexical_less_t
 \brief Less-than order on nodes based on lexical ordering
*/
class node_lexical_less_t {
public:
  /*!
   \brief Less-than order on nodes based on lexical ordering
   \param n1 : a node
   \param n2 : a node
   \return true if n1 is less-than n2 w.r.t. lexical ordering over the states in
   the nodes
  */
  bool operator()(tchecker::tck_reach::zg_covreach::graph_t::node_sptr_t const & n1,
                  tchecker::tck_reach::zg_covreach::graph_t::node_sptr_t const & n2) const
  {
    int state_cmp = tchecker::zg::lexical_cmp(n1->state(), n2->state());
    if (state_cmp != 0)
      return (state_cmp < 0);
    return (tchecker::graph::lexical_cmp(static_cast<tchecker::graph::node_flags_t const &>(*n1),
                                         static_cast<tchecker::graph::node_flags_t const &>(*n2)) < 0);
  }
};

/*!
 \class edge_lexical_less_t
 \brief Less-than ordering on edges based on lexical ordering
 */
class edge_lexical_less_t {
public:
  /*!
   \brief Less-than ordering on edges based on lexical ordering
   \param e1 : an edge
   \param e2 : an edge
   \return true if e1 is less-than  e2 w.r.t. the tuple of edges in e1 and e2
  */
  bool operator()(tchecker::tck_reach::zg_covreach::graph_t::edge_sptr_t const & e1,
                  tchecker::tck_reach::zg_covreach::graph_t::edge_sptr_t const & e2) const
  {
    return tchecker::lexical_cmp(e1->vedge(), e2->vedge()) < 0;
  }
};

std::ostream & dot_output(std::ostream & os, tchecker::tck_reach::zg_covreach::graph_t const & g, std::string const & name)
{
  return tchecker::graph::subsumption::dot_output<tchecker::tck_reach::zg_covreach::graph_t,
                                                  tchecker::tck_reach::zg_covreach::node_lexical_less_t,
                                                  tchecker::tck_reach::zg_covreach::edge_lexical_less_t>(os, g, name);
}

/* state_space_t */

state_space_t::state_space_t(std::shared_ptr<tchecker::zg::zg_t> const & zg, std::size_t block_size, std::size_t table_size)
    : _g_cache(std::make_shared<tchecker::tck_reach::zg_covreach::g_simulation_cache_t>(zg)),
      _ss(zg, zg, _g_cache, block_size, table_size)
{
}

tchecker::zg::zg_t & state_space_t::zg() { return _ss.ts(); }

tchecker::tck_reach::zg_covreach::graph_t & state_space_t::graph() { return _ss.state_space(); }

/* counter example */
namespace cex {

tchecker::tck_reach::zg_covreach::cex::symbolic_cex_t *
symbolic_counter_example(tchecker::tck_reach::zg_covreach::graph_t const & g)
{
  return tchecker::tck_reach::symbolic_counter_example_zg<tchecker::tck_reach::zg_covreach::graph_t>(g);
}

std::ostream & dot_output(std::ostream & os, tchecker::tck_reach::zg_covreach::cex::symbolic_cex_t const & cex,
                          std::string const & name)
{
  return tchecker::zg::path::symbolic::dot_output(os, cex, name);
}

tchecker::tck_reach::zg_covreach::cex::concrete_cex_t *
concrete_counter_example(tchecker::tck_reach::zg_covreach::graph_t const & g)
{
  return tchecker::tck_reach::concrete_counter_example_zg<tchecker::tck_reach::zg_covreach::graph_t>(g);
}

std::ostream & dot_output(std::ostream & os, tchecker::tck_reach::zg_covreach::cex::concrete_cex_t const & cex,
                          std::string const & name)
{
  return tchecker::zg::path::concrete::dot_output(os, cex, name);
}

} // namespace cex

/* run */

std::tuple<tchecker::algorithms::covreach::stats_t, std::shared_ptr<tchecker::tck_reach::zg_covreach::state_space_t>>
run(tchecker::parsing::system_declaration_t const & sysdecl, std::string const & labels, std::string const & search_order,
    tchecker::algorithms::covreach::covering_t covering, std::size_t block_size, std::size_t table_size)
{
  std::shared_ptr<tchecker::ta::system_t const> system{new tchecker::ta::system_t{sysdecl}};
  if (!tchecker::system::every_process_has_initial_location(system->as_system_system()))
    std::cerr << tchecker::log_warning << "system has no initial state" << std::endl;

  std::shared_ptr<tchecker::zg::zg_t> zg{tchecker::zg::factory(system, tchecker::ts::SHARING, tchecker::zg::ELAPSED_SEMANTICS,
                                                               tchecker::zg::NO_EXTRAPOLATION, block_size, table_size)};

  std::shared_ptr<tchecker::tck_reach::zg_covreach::state_space_t> state_space =
      std::make_shared<tchecker::tck_reach::zg_covreach::state_space_t>(zg, block_size, table_size);

  boost::dynamic_bitset<> accepting_labels = system->as_syncprod_system().labels(labels);

  enum tchecker::waiting::policy_t policy = tchecker::algorithms::fast_remove_waiting_policy(search_order);

  tchecker::algorithms::covreach::stats_t stats;
  tchecker::tck_reach::zg_covreach::algorithm_t algorithm;

  if (covering == tchecker::algorithms::covreach::COVERING_FULL)
    stats = algorithm.run<tchecker::algorithms::covreach::COVERING_FULL>(state_space->zg(), state_space->graph(),
                                                                         accepting_labels, policy);
  else if (covering == tchecker::algorithms::covreach::COVERING_LEAF_NODES)
    stats = algorithm.run<tchecker::algorithms::covreach::COVERING_LEAF_NODES>(state_space->zg(), state_space->graph(),
                                                                               accepting_labels, policy);
  else
    throw std::invalid_argument("Unknown covering policy for covreach algorithm");

  return std::make_tuple(stats, state_space);
}

} // namespace zg_covreach

} // end of namespace tck_reach

} // end of namespace tchecker
