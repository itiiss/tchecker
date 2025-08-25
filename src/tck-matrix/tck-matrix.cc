/*
 * This file is a part of the TChecker project.
 *
 * See files AUTHORS and LICENSE for copyright details.
 */

#include <iostream>
#include <getopt.h>
#include <memory>
#include <string>
#include <fstream>

#include "tchecker/parsing/parsing.hh"
#include "tchecker/system/system.hh"
#include "tchecker/ta/system.hh"
#include "tchecker/ta/ta.hh"
#include "tchecker/utils/log.hh"
#include "tchecker/utils/matrix_visualizer.hh"
#include "tchecker/zg/semantics.hh"
#include "tchecker/zg/state.hh"
#include "tchecker/zg/transition.hh"
#include "tchecker/zg/zg.hh"

/*!
 \file tck-matrix.cc
 \brief TChecker zone matrix visualization tool
 */

static struct option const long_options[] = {{"help", no_argument, 0, 'h'},
                                             {"output", required_argument, 0, 'o'},
                                             {"initial", no_argument, 0, 'i'},
                                             {"detailed", no_argument, 0, 'd'},
                                             {"simulation", required_argument, 0, 's'},
                                             {0, 0, 0, 0}};

static char const * const options = "ho:ids:";

void usage(char * progname)
{
  std::cerr << "Usage: " << progname << " [options] [file]" << std::endl;
  std::cerr << "   -h                     help" << std::endl;
  std::cerr << "   -o out_file            output file (default is standard output)" << std::endl;
  std::cerr << "   -i                     show initial zone matrix only" << std::endl;
  std::cerr << "   -d                     detailed output (both constraints and matrix)" << std::endl;
  std::cerr << "   -s simulation_steps    perform simulation with given steps and show zone matrices" << std::endl;
  std::cerr << "reads from standard input if file is not provided" << std::endl;
}

int main(int argc, char * argv[])
{
  int c;
  std::string output_file = "";
  bool initial_only = false;
  bool detailed_output = false;
  std::string simulation_steps = "";

  while ((c = getopt_long(argc, argv, options, long_options, nullptr)) != -1) {
    switch (c) {
    case 'h':
      usage(argv[0]);
      return EXIT_SUCCESS;
    case 'o':
      output_file = optarg;
      break;
    case 'i':
      initial_only = true;
      break;
    case 'd':
      detailed_output = true;
      break;
    case 's':
      simulation_steps = optarg;
      break;
    default:
      usage(argv[0]);
      return EXIT_FAILURE;
    }
  }

  std::string input_file = "";
  if (optind < argc)
    input_file = argv[optind];

  try {
    std::shared_ptr<tchecker::parsing::system_declaration_t> sysdecl{nullptr};

    if (input_file == "") {
      sysdecl = tchecker::parsing::parse_system_declaration(stdin, "");
      if (tchecker::log_error_count() > 0)
        return EXIT_FAILURE;
    } else {
      sysdecl = tchecker::parsing::parse_system_declaration(input_file);
      if (tchecker::log_error_count() > 0)
        return EXIT_FAILURE;
    }

    std::shared_ptr<tchecker::ta::system_t const> system{new tchecker::ta::system_t{*sysdecl}};

    // Set output stream
    std::ostream * os = &std::cout;
    std::unique_ptr<std::ofstream> fos;
    if (output_file != "") {
      fos = std::make_unique<std::ofstream>(output_file);
      if (!fos->is_open()) {
        std::cerr << "ERROR: cannot open " << output_file << " for writing" << std::endl;
        return EXIT_FAILURE;
      }
      os = fos.get();
    }

    // Create zone graph
    std::shared_ptr<tchecker::zg::zg_t> zg{tchecker::zg::factory(system, tchecker::ts::NO_SHARING,
                                                                 tchecker::zg::ELAPSED_SEMANTICS, 
                                                                 tchecker::zg::NO_EXTRAPOLATION, 10000, 65536)};

    if (zg.get() == nullptr) {
      std::cerr << "ERROR: unable to create zone graph" << std::endl;
      return EXIT_FAILURE;
    }

    // Get initial state  
    std::vector<tchecker::zg::zg_t::sst_t> initial_states;
    zg->initial(initial_states);
    if (initial_states.empty()) {
      std::cerr << "ERROR: no initial state found" << std::endl;
      return EXIT_FAILURE;
    }

    *os << "=== TChecker Zone Matrix Visualization ===" << std::endl;
    *os << "System: " << sysdecl->name() << std::endl;
    *os << "Clock dimension: " << system->clock_variables().size(tchecker::VK_DECLARED) + 1 << std::endl;
    *os << std::endl;

    // Create clock name function from system  
    auto clock_name = [&](tchecker::clock_id_t id) -> std::string {
      if (id == 0) return "0";
      auto const & clock_vars = system->clock_variables();
      if (id <= clock_vars.size(tchecker::VK_DECLARED)) {
        return clock_vars.name(id - 1);
      }
      return "c" + std::to_string(id);
    };
    
    // Get clock dimension
    tchecker::clock_id_t dim = system->clock_variables().size(tchecker::VK_DECLARED) + 1;

    auto initial_state = std::get<1>(initial_states[0]);
    
    if (detailed_output) {
      *os << "Initial State (Detailed):" << std::endl;
      *os << "Constraints: ";
      tchecker::dbm::output(*os, initial_state->zone().dbm(), initial_state->zone().dim(), clock_name);
      *os << std::endl;
      tchecker::utils::matrix_visualizer_t::output_dbm_with_labels(*os, initial_state->zone().dbm(), initial_state->zone().dim(), clock_name);
    } else {
      *os << "Initial Zone Matrix:" << std::endl;
      tchecker::utils::matrix_visualizer_t::output_dbm_with_labels(*os, initial_state->zone().dbm(), initial_state->zone().dim(), clock_name);
    }
    *os << std::endl;

    if (initial_only)
      return EXIT_SUCCESS;

    // Explore more states if requested
    if (!simulation_steps.empty()) {
      int max_steps = std::stoi(simulation_steps);
      *os << "=== Simulation (" << max_steps << " steps) ===" << std::endl;
      
      auto current_state = initial_state;
      int step = 0;
      
      while (step < max_steps) {
        // Get outgoing transitions
        std::vector<tchecker::zg::zg_t::sst_t> successors;
        // Cast to const state pointer for API compatibility
        tchecker::zg::const_state_sptr_t const_state(current_state.ptr());
        zg->next(const_state, successors);
        
        if (!successors.empty()) {
          auto successor = successors[0]; // Take first successor
          step++;
          *os << "Step " << step << ":" << std::endl;
          auto transition = std::get<2>(successor);
          *os << "Transition: <event>" << std::endl;
          
          auto successor_state = std::get<1>(successor);
          if (detailed_output) {
            *os << "Location: ";
            for (tchecker::process_id_t p = 0; p < system->processes_count(); ++p) {
              if (p > 0) *os << ",";
              *os << system->process_name(p) << ":loc" << successor_state->vloc()[p];
            }
            *os << " ";
            *os << "Constraints: ";
            tchecker::dbm::output(*os, successor_state->zone().dbm(), successor_state->zone().dim(), clock_name);
            *os << std::endl;
            tchecker::utils::matrix_visualizer_t::output_dbm_with_labels(*os, successor_state->zone().dbm(), successor_state->zone().dim(), clock_name);
          } else {
            tchecker::utils::matrix_visualizer_t::output_dbm_with_labels(*os, successor_state->zone().dbm(), successor_state->zone().dim(), clock_name);
          }
          *os << std::endl;
          
          current_state = successor_state;
        } else {
          break; // No more successors
        }
      }
    }

    return EXIT_SUCCESS;
  } catch (std::exception const & e) {
    std::cerr << "ERROR: " << e.what() << std::endl;
    return EXIT_FAILURE;
  }
}