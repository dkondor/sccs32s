/*
 * sccs32s.cpp -- calculate connected components for an undirected graph
 * 	use a very simple algorithm with iterative updates
 * 	use 32-bit IDs to reduce memory need, so it can work for very large graphs
 * 	use swappable memory by creating a temp file mapping to store the network
 *  during calculations, so it can work for graphs which do not fit in the
 *  memory (although performance will be less, but reads should be sequential
 *  so not that bad, but best if using SSD)
 * 
 * complications / gotchas:
 *   -- (maximum possible) number of edges need to be known in advance
 *   -- each edge should be unique on the input as this is not checked (although
 * 		it is not a problem if edges appear more than once, but will increase
 * 		computational time)
 * 	 -- not easy to limit computational complexity, probably less efficient
 * 		than the usual approach of using BFS / DFS
 * 
 * 
 * Copyright 2016,2018 Kondor Dániel <dkondor@mit.edu>
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above
 *   copyright notice, this list of conditions and the following disclaimer
 *   in the documentation and/or other materials provided with the
 *   distribution.
 * * Neither the name of the  nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * 
 */


#include <stdio.h>
#include <stdint.h>
#include <vector>
#include <unordered_map> // needs c++11
#include <time.h>

/* use POSIX functions for memory management */
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "read_table.h"

//~ using namespace std;

/* 
 * compute non-trivial hash of a 32-bit unsigned integer
 * 
 * main motivation: the integer hash functions provided by STL with g++
 * are a no-op, which can cause problems if the node IDs do not have good
 * randomness in the low bits
 * 
 * this is the case e.g. for Twitter tweet IDs, which results in a huge
 * amount of hash collisions
 */
struct ch32 {
	/* taken from
	 * https://stackoverflow.com/questions/664014/what-integer-hash-function-are-good-that-accepts-an-integer-hash-key
	 */
	size_t operator()(uint32_t x_) const {
		size_t x = x_;
		x = ((x >> 16) ^ x) * 0x45d9f3b;
	    x = ((x >> 16) ^ x) * 0x45d9f3b;
	    x = (x >> 16) ^ x;
	    return x;
	}
};

/* read graph (list of edges), maximum N edges */
uint64_t read_graph(uint32_t* i1, uint32_t* i2, FILE* f, uint64_t N) {
	read_table2 r(f);
	uint64_t i = 0;
	while(r.read_line()) {
		if(!r.read(i1[i],i2[i])) {
			if(r.get_last_error() == T_OVERFLOW) continue; // ignore overflow / negative values
			break;
		}
		i++;
	}
	if(r.get_last_error() != T_EOF) {
		r.write_error(stderr);
		return 0;
	}
	return i;
}



int main(int argc, char **argv)
{
	uint64_t n1 = 0;
	char* tmpfn = 0;
	bool use_reverse_map = false;
	
	for(int i=1;i<argc;i++) if(argv[i][0] == '-') switch(argv[i][1]) {
		case 'N': /* maximum number of edges in the graph, need to be given */
			n1 = strtoul(argv[i+1],0,10);
			break;
		case 't': /* filename to use as temporary file */
			tmpfn = argv[i+1]; /* if not given, just use RAM */
			break;
		case 'r':
			use_reverse_map = true;
			break;
		default:
			fprintf(stderr,"Unknown parameter: %s!\n",argv[i]);
			break;
	}
	
	if(n1 == 0) {
		fprintf(stderr,"Error: no buffer size specified!\n");
		return 1;
	}
	
	void* buf = MAP_FAILED;
	uint64_t s = n1*2*sizeof(uint32_t);
	int f = -1;
	
	if(tmpfn) {
		/* note: O_EXCL makes sure the file does not already exist */
		f = open(tmpfn,O_CREAT | O_RDWR | O_EXCL,S_IRUSR | S_IWUSR);
		if(f == -1) {
			fprintf(stderr,"Error opening temporary file %s!\n",tmpfn);
			return 2;
		}
		if(ftruncate(f,s) == -1) {
			fprintf(stderr,"Error setting file size on temporary file %s to %lu!\n",tmpfn,s);
			close(f);
			return 3;
		}
		buf = mmap(0,s,PROT_READ | PROT_WRITE,MAP_SHARED,f,0);
		if(buf == MAP_FAILED) {
			fprintf(stderr,"Error creating buffers from file %s!\n",tmpfn);
		}
		unlink(tmpfn); /* note: do not keep the temporary file
			-- might be confusing for some people that it's still taking up disk space? */
	}
	else {
		/* just allocate a lot of memory */
		buf = mmap(0,s,PROT_READ | PROT_WRITE,MAP_PRIVATE | MAP_ANONYMOUS,-1,0);
		if(buf == MAP_FAILED) {
			fprintf(stderr,"Error allocating memory for the buffers!\n");
			return 11;
		}
	}
	
	uint32_t* u1 = (uint32_t*)buf;
	uint32_t* u2 = u1 + n1;
	
	time_t t1;
	
	t1 = time(0);
	fprintf(stderr,"%sreading input\n",ctime(&t1));
	
	uint64_t n = read_graph(u1,u2,stdin,n1);
	if(n == 0) return 1;

	t1 = time(0);
	fprintf(stderr,"%s%lu edges read\n",ctime(&t1),n);
	
	/* assignement of users to sccs -- key is userid, stored value is sccid */
	std::unordered_map<uint32_t,uint32_t,ch32> sccs;
	std::unordered_map<uint32_t,uint32_t,ch32> merge; //keep track of sccs to merge
	/* optional extra copy of assignement of users to sccs
	 *  -- key is sccid, stored value is userid
	 * used to be able to update sccs more efficiently */
	std::unordered_multimap<uint32_t,uint32_t,ch32> sccs2;
	
	//1. just get all users
	for(uint64_t i=0;i<n;i++) {
		/* note: at first each user is in a separate scc, so the sccs
		 * multimap can be used to find all unique userids */
		auto it = sccs.find(u1[i]);
		if(it == sccs.end()) sccs.insert(std::make_pair(u1[i],u1[i]));
		it = sccs.find(u2[i]);
		if(it == sccs.end()) sccs.insert(std::make_pair(u2[i],u2[i]));
	}
	
	t1 = time(0);
	fprintf(stderr,"%s%lu users in total\n",ctime(&t1),sccs.size());
	
	//2. iteratively update sccs assignements, always try to lower scc ids
	unsigned int j = 0;
	uint64_t k = 0;
	while(1) {
		for(uint64_t i=0;i<n;i++) {
			uint32_t i1 = sccs[u1[i]];
			uint32_t i2 = sccs[u2[i]];
			
			while(i2 == i1 && i<n) {
				/* remove edges where both addresses already were assigned to
				 * the same scc -- these will not affect the result anymore */
				u1[i] = u1[n-1];
				u2[i] = u2[n-1];
				i1 = sccs[u1[i]];
				i2 = sccs[u2[i]];
				n--;
			}
			if(i == n) break; /* no more edges to process */
			if(i2 < i1) {
				uint32_t tmp = i2;
				i2 = i1;
				i1 = tmp;
			}
			
			// add to the list of merges
			auto it = merge.find(i2);
			if(it == merge.end()) merge.insert(std::make_pair(i2,i1));
			else if(i1 < it->second) it->second = i1;
		}
		
		if(merge.size() == 0) break; //no more updates to do
		
		/* go through all updates to do, find the minimum for each SCC edge */
		{
			std::vector<typename std::unordered_map<unsigned int,unsigned int,ch32>::iterator> updates;
			for(auto it = merge.begin();it!=merge.end();++it) {
				auto it1 = it;
				auto it2 = merge.find(it1->second);
				while(it2 != merge.end()) {
					updates.push_back(std::move(it1));
					it1 = it2;
					it2 = merge.find(it1->second);
				}
				unsigned int idlast = it1->second;
				while(!updates.empty()) {
					updates.back()->second = idlast;
					updates.pop_back();
				}
			}
		}
		
		/* do the updates; simple version which iterates over all users,
		 * this could be improved by sorting them by sccid */
		if(sccs2.size() == 0) for(auto it = sccs.begin(); it != sccs.end(); ++it) {
			uint32_t sccid = it->second;
			auto it2 = merge.find(sccid);
			if(it2 != merge.end()) { it->second = it2->second; k++; }
			/* create the reverse map during the first pass */
			if(use_reverse_map) sccs2.insert(std::make_pair(it->second,it->first));
		}
		/* improved version: scc ids can be searched in the sccs multimap */
		else for(const auto& sccedge : merge) {
			/* replace sccedge.first with sccedge.second everywhere */
			/* note: use C++17 style node access and modification -- does not work until gcc 7.1
			auto x = sccs2.extract(sccedge.first);
			if(x.empty()) {
				fprintf(stderr,"Inconsistent scc mappings: scc %u has no users in it!\n",sccedge.first);
				k = 0;
				break;
			}
			do {
				x.key = sccedge.second;
				sccs[x.value] = sccedge.second;
				sccs2.insert(x);
				k++;
				x = sccs2.extract(sccedge.first);
			} while(!x.empty()); */
			auto it = sccs2.find(sccedge.first);
			if(it == sccs2.end()) {
				fprintf(stderr,"Inconsistent scc mappings: scc %u has no users in it!\n",sccedge.first);
				k = 0;
				break;
			}
			do {
				std::pair<unsigned int,unsigned int> x = *it; /* note: this creates a copy */
				sccs2.erase(it);
				x.first = sccedge.second;
				sccs[x.second] = sccedge.second;
				sccs2.insert(x);
				k++;
				it = sccs2.find(sccedge.first);
			} while(it != sccs2.end());
		}
		if(k == 0) { j = 0; break; } /* error occured previously */
		
		j++;
		t1 = time(0);
		fprintf(stderr,"%siteration %u, %lu edges remain, %lu sccs / %lu users updated\n",ctime(&t1),j,n,merge.size(),k);
		merge.clear();
		k = 0;
	}
	
	t1 = time(0);
	fprintf(stderr,"%sdone processing\n",ctime(&t1));
	
	if(j == 0) fprintf(stderr,"Error encountered during processing!\n");
	// write output
	else for(auto it = sccs.begin(); it != sccs.end(); ++it) {
		fprintf(stdout,"%u\t%u\n",it->first,it->second);
	}
	
	munmap(buf,s);
	if(tmpfn) close(f);
	
	return 0;
}

