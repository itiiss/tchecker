/*
 * This file is a part of the TChecker project.
 *
 * See files AUTHORS and LICENSE for copyright details.
 */

#include <cassert>
#include <iomanip>
#include <sstream>
#include <stdexcept>

#include "tchecker/utils/matrix_visualizer.hh"

namespace tchecker {

namespace utils {

std::ostream & matrix_visualizer_t::output_zone_matrix(std::ostream & os, tchecker::zg::zone_t const & zone,
                                                       tchecker::clock_index_t const & index)
{
  return output_dbm_with_labels(os, zone.dbm(), zone.dim(), index);
}

std::ostream & matrix_visualizer_t::output_dbm_with_labels(std::ostream & os, tchecker::dbm::db_t const * dbm,
                                                           tchecker::clock_id_t dim,
                                                           tchecker::clock_index_t const & index)
{
  return output_dbm_with_labels(os, dbm, dim,
                                [&](tchecker::clock_id_t id) { return (id == 0 ? "0" : index.value(id - 1)); });
}

std::ostream & matrix_visualizer_t::output_dbm_with_labels(std::ostream & os, tchecker::dbm::db_t const * dbm,
                                                           tchecker::clock_id_t dim,
                                                           std::function<std::string(tchecker::clock_id_t)> clock_name)
{
  assert(dbm != nullptr);
  assert(dim >= 1);

  // Output header
  output_matrix_header(os, dim, clock_name);
  output_matrix_separator(os, dim);

  // Output matrix rows with labels
  for (tchecker::clock_id_t i = 0; i < dim; ++i) {
    os << std::setw(6) << clock_name(i) << " |";
    for (tchecker::clock_id_t j = 0; j < dim; ++j) {
      os << std::setw(8);
      tchecker::dbm::output(os, dbm[i * dim + j]);
      if (j < dim - 1)
        os << " ";
    }
    os << std::endl;
  }

  return os;
}

std::ostream & matrix_visualizer_t::output_zone_comparison(std::ostream & os, tchecker::zg::zone_t const & zone1,
                                                           tchecker::zg::zone_t const & zone2,
                                                           tchecker::clock_index_t const & index)
{
  if (zone1.dim() != zone2.dim()) {
    throw std::invalid_argument("Zone dimensions must match for comparison");
  }

  os << "Zone 1:" << std::endl;
  output_zone_matrix(os, zone1, index);
  
  os << std::endl << "Zone 2:" << std::endl;
  output_zone_matrix(os, zone2, index);

  return os;
}

std::ostream & matrix_visualizer_t::output_zone_detailed(std::ostream & os, tchecker::zg::zone_t const & zone,
                                                         tchecker::clock_index_t const & index)
{
  os << "Zone (Constraint form): ";
  zone.output(os, index);
  os << std::endl << std::endl;

  os << "Zone (Matrix form):" << std::endl;
  output_zone_matrix(os, zone, index);

  return os;
}

std::string matrix_visualizer_t::zone_to_matrix_string(tchecker::zg::zone_t const & zone,
                                                       tchecker::clock_index_t const & index)
{
  std::stringstream ss;
  output_zone_matrix(ss, zone, index);
  return ss.str();
}

void matrix_visualizer_t::output_matrix_header(std::ostream & os, tchecker::clock_id_t dim,
                                               std::function<std::string(tchecker::clock_id_t)> clock_name)
{
  os << std::setw(6) << "" << " |";
  for (tchecker::clock_id_t j = 0; j < dim; ++j) {
    os << std::setw(8) << clock_name(j);
    if (j < dim - 1)
      os << " ";
  }
  os << std::endl;
}

void matrix_visualizer_t::output_matrix_separator(std::ostream & os, tchecker::clock_id_t dim)
{
  os << std::string(7, '-') << "+";
  for (tchecker::clock_id_t j = 0; j < dim; ++j) {
    os << std::string(8, '-');
    if (j < dim - 1)
      os << "-";
  }
  os << std::endl;
}

} // end of namespace utils

} // end of namespace tchecker