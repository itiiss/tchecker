/*
 * This file is a part of the TChecker project.
 *
 * See files AUTHORS and LICENSE for copyright details.
 */

#ifndef TCHECKER_UTILS_MATRIX_VISUALIZER_HH
#define TCHECKER_UTILS_MATRIX_VISUALIZER_HH

#include <functional>
#include <iostream>

#include "tchecker/basictypes.hh"
#include "tchecker/dbm/dbm.hh"
#include "tchecker/variables/clocks.hh"
#include "tchecker/zg/zone.hh"

/*!
 \file matrix_visualizer.hh
 \brief Utilities for visualizing DBMs and zones as matrices
 */

namespace tchecker {

namespace utils {

/*!
 \class matrix_visualizer_t
 \brief Matrix visualization utilities for DBMs and zones
 */
class matrix_visualizer_t {
public:
  /*!
   \brief Output zone as labeled DBM matrix
   \param os : output stream
   \param zone : a zone
   \param index : clock index (map clock ID -> clock name)
   \pre index is a clock index over system clocks (the first clock has index 0)
   \post zone's DBM has been output to os in matrix format with clock labels
   \return os after output
   */
  static std::ostream & output_zone_matrix(std::ostream & os, tchecker::zg::zone_t const & zone,
                                           tchecker::clock_index_t const & index);

  /*!
   \brief Output DBM as labeled matrix
   \param os : output stream
   \param dbm : a DBM
   \param dim : dimension of dbm
   \param index : clock index (map clock ID -> clock name)
   \pre dbm is not nullptr (checked by assertion)
   dbm is a dim*dim array of difference bounds
   dim >= 1 (checked by assertion)
   index is a clock index over system clocks (the first clock has index 0)
   \post dbm has been output to os in matrix format with clock labels
   \return os after output
   */
  static std::ostream & output_dbm_with_labels(std::ostream & os, tchecker::dbm::db_t const * dbm,
                                               tchecker::clock_id_t dim, tchecker::clock_index_t const & index);

  /*!
   \brief Output DBM as labeled matrix with custom clock naming function
   \param os : output stream
   \param dbm : a DBM
   \param dim : dimension of dbm
   \param clock_name : function mapping clock ID to clock name
   \pre dbm is not nullptr (checked by assertion)
   dbm is a dim*dim array of difference bounds
   dim >= 1 (checked by assertion)
   clock_name maps any clock ID in [0,dim) to a name
   \post dbm has been output to os in matrix format with clock labels
   \return os after output
   */
  static std::ostream & output_dbm_with_labels(std::ostream & os, tchecker::dbm::db_t const * dbm,
                                               tchecker::clock_id_t dim,
                                               std::function<std::string(tchecker::clock_id_t)> clock_name);

  /*!
   \brief Output zone comparison as side-by-side matrices
   \param os : output stream
   \param zone1 : first zone
   \param zone2 : second zone
   \param index : clock index (map clock ID -> clock name)
   \pre zone1 and zone2 have the same dimension
   index is a clock index over system clocks (the first clock has index 0)
   \post both zones' DBMs have been output to os as side-by-side matrices
   \return os after output
   \throw std::invalid_argument : if zones have different dimensions
   */
  static std::ostream & output_zone_comparison(std::ostream & os, tchecker::zg::zone_t const & zone1,
                                               tchecker::zg::zone_t const & zone2, tchecker::clock_index_t const & index);

  /*!
   \brief Output zone with both constraint and matrix representations
   \param os : output stream
   \param zone : a zone
   \param index : clock index (map clock ID -> clock name)
   \pre index is a clock index over system clocks (the first clock has index 0)
   \post zone has been output to os in both constraint and matrix formats
   \return os after output
   */
  static std::ostream & output_zone_detailed(std::ostream & os, tchecker::zg::zone_t const & zone,
                                             tchecker::clock_index_t const & index);

  /*!
   \brief Create a string representation of zone as matrix
   \param zone : a zone
   \param index : clock index (map clock ID -> clock name)
   \pre index is a clock index over system clocks (the first clock has index 0)
   \return string representation of zone's DBM as labeled matrix
   */
  static std::string zone_to_matrix_string(tchecker::zg::zone_t const & zone, tchecker::clock_index_t const & index);

private:
  /*!
   \brief Helper function to output matrix header
   \param os : output stream
   \param dim : matrix dimension
   \param clock_name : function mapping clock ID to clock name
   */
  static void output_matrix_header(std::ostream & os, tchecker::clock_id_t dim,
                                   std::function<std::string(tchecker::clock_id_t)> clock_name);

  /*!
   \brief Helper function to output matrix separator line
   \param os : output stream
   \param dim : matrix dimension
   */
  static void output_matrix_separator(std::ostream & os, tchecker::clock_id_t dim);
};

} // end of namespace utils

} // end of namespace tchecker

#endif // TCHECKER_UTILS_MATRIX_VISUALIZER_HH