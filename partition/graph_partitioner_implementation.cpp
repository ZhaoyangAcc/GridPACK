/*
 *     Copyright (c) 2013 Battelle Memorial Institute
 *     Licensed under modified BSD License. A copy of this license can be found
 *     in the LICENSE file in the top level directory of this distribution.
 */
// -------------------------------------------------------------
/**
 * @file   graph_partitioner_implementation.cpp
 * @author William A. Perkins
 * @date   2014-02-25 15:50:30 d3g096
 * 
 * @brief  
 * 
 * 
 */
// -------------------------------------------------------------

#include <algorithm>
#include <set>
#include <ga++.h>
#include <boost/mpi/collectives.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/lambda/lambda.hpp>
#include <boost/lambda/bind.hpp>
#include <boost/serialization/utility.hpp>
#include <boost/serialization/set.hpp>
#include <boost/format.hpp>

#include "gridpack/utilities/exception.hpp"
#include "graph_partitioner_implementation.hpp"



namespace gridpack {
namespace network {

// -------------------------------------------------------------
//  class GraphPartitionerImplementation
// -------------------------------------------------------------

// -------------------------------------------------------------
// GraphPartitionerImplementation:: constructors / destructor
// -------------------------------------------------------------
GraphPartitionerImplementation::GraphPartitionerImplementation(const parallel::Communicator& comm)
  : parallel::Distributed(comm), utility::Uncopyable(),
    p_adjacency_list(comm), 
    p_node_destinations(),
    p_edge_destinations()
{
  // empty
}

GraphPartitionerImplementation::GraphPartitionerImplementation(const parallel::Communicator& comm,
                                                               const int& local_nodes, 
                                                               const int& local_edges)
  : parallel::Distributed(comm), utility::Uncopyable(),
    p_adjacency_list(comm, local_nodes, local_edges), 
    p_node_destinations(local_nodes),
    p_edge_destinations(local_edges)
{
  // empty
}

GraphPartitionerImplementation::~GraphPartitionerImplementation(void)
{
}

// -------------------------------------------------------------
// GraphPartitionerImplementation::node_destinations
// -------------------------------------------------------------
void
GraphPartitionerImplementation::node_destinations(GraphPartitionerImplementation::IndexVector& dest) const
{
  dest.clear();
  std::copy(p_node_destinations.begin(), p_node_destinations.end(),
            std::back_inserter(dest));
}

// -------------------------------------------------------------
// GraphPartitionerImplementation::edge_destinations
// -------------------------------------------------------------
void
GraphPartitionerImplementation::edge_destinations(GraphPartitionerImplementation::IndexVector& dest) const
{
  dest.clear();
  std::copy(p_edge_destinations.begin(), p_edge_destinations.end(),
            std::back_inserter(dest));
}

// -------------------------------------------------------------
// GraphPartitionerImplementation::ghost_node_destinations
// -------------------------------------------------------------
void
GraphPartitionerImplementation::ghost_node_destinations(GraphPartitionerImplementation::MultiIndexVector& dest) const
{
  dest.clear();
  std::copy(p_ghost_node_destinations.begin(), 
            p_ghost_node_destinations.end(),
            std::back_inserter(dest));
}

// -------------------------------------------------------------
// GraphPartitionerImplementation::ghost_edge_destinations
// -------------------------------------------------------------
void
GraphPartitionerImplementation::ghost_edge_destinations(GraphPartitionerImplementation::IndexVector& dest) const
{
  dest.clear();
  std::copy(p_ghost_edge_destinations.begin(), 
            p_ghost_edge_destinations.end(),
            std::back_inserter(dest));
}

// -------------------------------------------------------------
// GraphPartitionerImplementation::partition
// -------------------------------------------------------------
void
GraphPartitionerImplementation::partition(void)
{
  static const bool verbose(false);
  p_adjacency_list.ready();

  int maxdim(2);
  int dims[maxdim], lo[maxdim], hi[maxdim], ld[maxdim];
  ld[0] = 1;
  ld[1] = 1;

  int locnodes(p_adjacency_list.nodes());
  int locedges(p_adjacency_list.edges());
  int allnodes;
  int alledges;

  communicator().barrier();
  boost::mpi::all_reduce(communicator(), 
                         locnodes, allnodes, std::plus<int>());
  boost::mpi::all_reduce(communicator(), 
                         locedges, alledges, std::plus<int>());

  if (allnodes <= 0 || alledges <= 0) {
    boost::format fmt("%d: GraphPartitioner::partition(): called without nodes (%d) or edges (%)");
    
    std::string msg = boost::str(fmt % communicator().worldRank() % allnodes % alledges);
    throw Exception(msg);
  }

  this->p_partition();          // fills p_node_destinations

  // make two GAs, one that holds the node source and another that
  // node destination; each is indexed by global node index

  int theGAgroup(communicator().getGroup());
  int oldGAgroup = GA_Pgroup_get_default();
  GA_Pgroup_set_default(theGAgroup);

  std::vector<int> nodeidx(locnodes);
  std::vector<int *> stupid(locnodes);
  for (Index n = 0; n < locnodes; ++n) {
    nodeidx[n] = p_adjacency_list.node_index(n);
    stupid[n] = &nodeidx[n];
  }

  

  dims[0] = allnodes;
  boost::scoped_ptr<GA::GlobalArray> 
    node_dest(new GA::GlobalArray(MT_C_INT, 1, dims, "Node Destinations Process", NULL)),
    node_src(new GA::GlobalArray(MT_C_INT, 1, dims, "Node Source Process", NULL));
  node_dest->scatter(&p_node_destinations[0], &stupid[0], locnodes);
  
  { 
    std::vector<int> nsrc(locnodes, this->processor_rank());
    node_src->scatter(&nsrc[0], &stupid[0], locnodes);
  }
  
  communicator().sync();

  if (verbose) {
    node_src->print();
    node_dest->print();
  }

  // edges are assigned to the same partition as the lowest numbered
  // node to which it connects, which are extracted from the node
  // destination GA.

  nodeidx.resize(locedges);
  stupid.resize(locedges);
  std::vector<int> e1dest(locedges);

  for (Index e = 0; e < locedges; ++e) {
    Index n1, n2;
    p_adjacency_list.edge(e, n1, n2);
    nodeidx[e] = std::min(n1, n2);
    stupid[e] = &nodeidx[e];
  }

  node_dest->gather(&e1dest[0], &stupid[0], locedges);

  if (verbose) {
    for (Index e = 0; e < locedges; ++e) {
      Index n1, n2;
      p_adjacency_list.edge(e, n1, n2);
      std::cout << processor_rank() << ": active edge " << e
                << " (" << n1 << "->" << n2 << "): "
                << "destination: " << e1dest[e] << std::endl;
    }
  }

  p_edge_destinations.clear();
  p_edge_destinations.reserve(locedges);
  std::copy(e1dest.begin(), e1dest.end(), 
            std::back_inserter(p_edge_destinations));

  // determine (possible) destinations for ghost edges (highest numbered node) 

  std::vector<int> e2dest(locedges);
  for (Index e = 0; e < locedges; ++e) {
    Index n1, n2;
    p_adjacency_list.edge(e, n1, n2);
    nodeidx[e] = std::max(n1, n2);
    stupid[e] = &nodeidx[e];
  }

  node_dest->gather(&e2dest[0], &stupid[0], locedges);
  
  if (verbose) {
    for (Index e = 0; e < locedges; ++e) {
      Index n1, n2;
      p_adjacency_list.edge(e, n1, n2);
      std::cout << processor_rank() << ": ghost edge " << e
                << " (" << n1 << "->" << n2 << "): "
                << "destination: " << e2dest[e] << std::endl;
    }
  }

  
  communicator().sync();

  // These are no longer needed

  node_dest.reset();
  node_src.reset();


  p_ghost_edge_destinations.reserve(locedges);
  std::copy(e2dest.begin(), e2dest.end(), 
            std::back_inserter(p_ghost_edge_destinations));

  // determine destinations for ghost nodes: go thru the edges and
  // compare destinations of connected nodes; if they're different,
  // then both ends need to be ghosted (to different processors)

  // a particular node and destination needs to be unique, hence the
  // use of set<>; this may be too slow with large networks and
  // processors

  typedef std::set< std::pair<Index, int> > DestList;
  DestList gnodedest;
  for (Index e = 0; e < locedges; ++e) {
    Index n1, n2;
    p_adjacency_list.edge(e, n1, n2);

    int n1dest(e1dest[e]);
    int n2dest(e2dest[e]);

    if (verbose) {
      std::cout << processor_rank() << ": edge " << e
                << " (" << n1 << "->" << n2 << "): "
                << "destinations: " << n1dest << ", " << n2dest << std::endl;
    }
      
    if (n1dest != n2dest) {
      gnodedest.insert(std::make_pair(std::min(n1,n2), n2dest));
      gnodedest.insert(std::make_pair(std::max(n1,n2), n1dest));
    }
  }

  if (verbose) {
    if (this->processor_rank() == 0) {
      std::cout << "Ghost node destinations: " << std::endl;
    }
    for (int p = 0; p < this->processor_size(); ++p) {
      if (this->processor_rank() == p) {
        std::cout << p << ": ";
        for (DestList::const_iterator i = gnodedest.begin();
             i != gnodedest.end(); ++i) {
          std::cout << "(" << i->first << ":" << i->second << "),";
        }
        std::cout << std::endl;
      }
      this->communicator().barrier();
    }
  }

  // It's possible that edges are distributed over multiple processes,
  // which could result in a different set of ghost destinations for a
  // given node on each process. These need to be put together.

  // In this approach, which is really slow, take each local list of
  // ghost node, send it to all processes. Each process extracts the
  // ghost node destination for its locally owned nodes.

  // p_ghost_node_destinations.resize(locnodes);
  // DestList tmp;
  // for (int p = 0; p < this->processor_size(); ++p) {
  //   tmp.clear();
  //   if (this->processor_rank() == p) {
  //     tmp = gnodedest;
  //   }
  //   broadcast(communicator().getCommunicator(), tmp, p);
  //   for (Index n = 0; n < locnodes; ++n) {
  //     Index nodeidx(p_adjacency_list.node_index(n));
  //     for (DestList::const_iterator i = tmp.begin();
  //          i != tmp.end(); ++i) {
  //       if (nodeidx == i->first) {
  //         p_ghost_node_destinations[n].push_back(i->second);
  //       }
  //     }
  //   }
  // }



  // Here, a 2D GA is used to store the ghost node destinations.  Each
  // process takes it's set of ghost node destinations and appends
  // those lists already in the GA.

  dims[0] = allnodes;
  dims[1] = processor_size();

  node_dest.reset(new GA::GlobalArray(MT_C_INT, 2, &dims[0], 
                                      "Ghost node dest processes", NULL));
  boost::scoped_ptr<GA::GlobalArray> 
    node_dest_count(new GA::GlobalArray(MT_C_INT, 1, &dims[0],
                                        "Ghost node dest count", NULL));

  {
    int bogus;
    bogus = -1; node_dest->fill(&bogus);
    bogus = 0; node_dest_count->fill(&bogus);
  }

  std::vector<int> lcount(allnodes, 0);
  for (int p = 0; p < this->processor_size(); ++p) {
    if (this->processor_rank() == p) {
      lo[0] = 0; hi[0] = allnodes - 1;
      node_dest_count->get(&lo[0], &hi[0], &lcount[0], &ld[0]);
      for (DestList::const_iterator i = gnodedest.begin();
           i != gnodedest.end(); ++i) {
        int nid(i->first), dest(i->second);
        lo[0] = nid;
        lo[1] = lcount[nid];
        hi[0] = nid;
        hi[1] = lcount[nid];
        node_dest->put(&lo[0], &hi[0], &dest, &ld[0]);
        lcount[nid] += 1;
      }
      lo[0] = 0; hi[0] = allnodes - 1;
      node_dest_count->put(&lo[0], &hi[0], &lcount[0], &ld[0]);
    }
    this->communicator().sync();
  }    


  // After all processes have made their contribution to the ghost
  // node destination GA, each process grabs that part that refers to
  // its local nodes and fills p_ghost_edge_destinations.

  lo[0] = 0; hi[0] = allnodes - 1;
  node_dest_count->get(&lo[0], &hi[0], &lcount[0], &ld[0]);

  p_ghost_node_destinations.clear();
  p_ghost_node_destinations.resize(locnodes);
  std::vector<int> tmpdest(this->processor_size(), 0);
  for (Index n = 0; n < locnodes; ++n) {
    Index nid(p_adjacency_list.node_index(n));
    p_ghost_node_destinations[n].clear();
    
    if (lcount[nid] > 0) {
      lo[0] = nid;
      hi[0] = nid;
      lo[1] = 0;
      hi[1] = lcount[nid] - 1;
      tmpdest.resize(lcount[nid]);
      node_dest->get(&lo[0], &hi[0], &tmpdest[0], &ld[0]);

      // there may be duplicates, so get rid of them
      if (tmpdest.size() > 1) {
        std::stable_sort(tmpdest.begin(), tmpdest.end());
        std::unique(tmpdest.begin(), tmpdest.end());
      }

      p_ghost_node_destinations[n].reserve(tmpdest.size());
      std::copy(tmpdest.begin(), tmpdest.end(),
                std::back_inserter(p_ghost_node_destinations[n]));
    }
  }
  


  GA_Pgroup_set_default(oldGAgroup);

}
} // namespace network
} // namespace gridpack
