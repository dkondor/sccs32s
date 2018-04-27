# sccs32s
Iterative method to find connected components in an undirected graph.

Originally optimized for low memory usage (in the case of large graphs), but
less efficient than the usual approach of using BFS / DFS.

Has the option to use a temporary file as "swap space" for the graph edges,
so graphs which do not fit in the memory can be used (but will be slower)

Has the following gotchas:
 - uses UNIX / POSIX specific functions for memory management
 - node IDs in the graph has to be 32-bit unsigned integers; negative numbers are silently ignored
 - number of edges need to be known in advance (given as a command line argument)
 - each edge should be unique on the input as this is not checked (while it is not a problem if edges appear more than once, but will increase computational time)


# Example usage:

Group Bitcoin addresses by appearing together as inputs of the same transaction;
i.e. if addresses A and B appear as inputs of TX1 and B, C and D as inputs of
TX2, all four addresses are grouped together as belonging to the same "entity"
that is assumed to control all of them to be able to create transactions with them.

Computationally, this corresponds to finding the connected components of an
auxiliary graph constructed as having an edge between addresses that appear as
inputs of the same transaction.


1. Create this auxiliary graph from transaction inputs
```
xzcat txin.dat.xz | awk 'BEGIN{txl=0;addrl=0;addrl2=0;}{if($1 == txl) { if(addrl != $5) print addrl,$5; if(addrl2 != $5) print addrl2,$5; } else  { txl = $1; addrl = $5;} addrl2 = $5;}' | uniq > addr_edges.dat
```
Note: a transaction with N unique input addresses would contribute N(N-1)/2 edges.
Since there are some transactions with a very large number of inputs, creating all
edges would result in a prohibitably huge graph. Instead, the above code creates
2(N-1) edges for each transaction, that still form a connected graph, so the connected
components of this sample graph will be the same as if we created the "full" graph.
Edges are created between the first address appearing as transaction input and any other
address and also between consecutive addresses appearing as inputs. Only one of these
options would be enough as well of course.


2. calculate connected components
first create unique edges
```
sort -S 20G addr_edges.dat | uniq > addr_edges_s.dat
wc -l addr_edges.dat
# 582933980 addr_edges.dat
wc -l addr_edges_s.dat
# 496529253 addr_edges_s.dat
```
find connected components with this program (note: invalid addresses (-1) will be ignored in the input)
```
./sccs32s -N 496529253 -t sccstmp -r < addr_edges_s.dat > addr_sccs.dat
```


3. compare results with the usual approach for calculating sccs
use the program from https://github.com/dkondor/graph_simple

this time, we need to filter invalid addresses separately
```
awk '{if($1!=-1 && $2!=-1) print $0;}' addr_edges.dat | graph_simple/sccs > addr_sccs5.dat
# 35660272 connected components found
```

compare the two results -- no output means OK
(note that SCC IDs will be different)
```
./sccscomp -1 addr_sccs.dat -2 addr_sccs5.dat
```


# Compilation


Should be very simple, but requires C++14. E.g. with gcc:
```
g++ -o sccs32s sccs32s.cpp -std=gnu++14 -O3 -march=native
g++ -o sccscomp sccs_compare.cpp -std=gnu++14 -O3 -march=native
```



