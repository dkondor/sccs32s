/*
 * sccs_compare.cpp -- compare two possible labelings of a graph
 * 
 * Copyright 2018 Daniel Kondor <kondor.dani@gmail.com>
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
 */


#include <stdio.h>
#include <vector>
#include <algorithm>
#include <utility>
#include "read_table.h"

int read_sccs(const char* fn, std::vector<std::pair<unsigned int,unsigned int> >& sccs) {
	FILE* f1 = stdin;
	if(fn) {
		f1 = fopen(fn,"r");
		if(!f1) {
			fprintf(stderr,"Error opening file %s!\n",fn);
			return 1;
		}
	}
	read_table2 rt(f1);
	if(fn) rt.set_fn(fn);
	while(rt.read_line()) {
		unsigned int x,y;
		if(!rt.read(x,y)) break;
		sccs.push_back(std::make_pair(x,y));
	}
	if(fn) fclose(f1);
	if(rt.get_last_error() != T_EOF) {
		fprintf(stderr,"Error reading input: ");
		rt.write_error(stderr);
		return 1;
	}
	return 0;
}

int main(int argc, char **argv)
{
	char* i1 = 0;
	char* i2 = 0;
	
	for(int i=1;i<argc;i++) if(argv[i][0] == '-') switch(argv[i][1]) {
		case '1':
			if(argv[i+1]) {
				if(argv[i+1][0] == '-' && argv[i+1][1] == 0) break;
				i1 = argv[i+1];
			}
			break;
		case '2':
			if(argv[i+1]) {
				if(argv[i+1][0] == '-' && argv[i+1][1] == 0) break;
				i2 = argv[i+1];
			}
			break;
		default:
			fprintf(stderr,"Unknown parameter: %s!\n",argv[i]);
			break;
	}
	
	if(i1 == 0 && i2 == 0) {
		fprintf(stderr,"No input files given!\n");
		return 1;
	}
	
	std::vector<std::pair<unsigned int,unsigned int> > sccs1;
	std::vector<std::pair<unsigned int,unsigned int> > sccs2;
	
	if(read_sccs(i1,sccs1) || read_sccs(i2,sccs2)) return 1;
	
	/* sort first list by addresses */
	std::sort(sccs1.begin(),sccs1.end(),[](const auto& a, const auto& b) { return a.first < b.first; });
	/* sort second list by sccs IDs */
	std::sort(sccs2.begin(),sccs2.end(),[](const auto& a, const auto& b) { return a.second < b.second; });
	
	unsigned int sccid2 = sccs2[0].second + 1; /* make sure it will not match */
	unsigned int sccid1 = 0;
	for(const auto& x : sccs2) {
		/* find address in sccs1
		 * note: total runtime will be ~N*log(N) because of the binary search
		 * could be faster using hashmaps
		 * but the above sort already need ~N*log(N) time, so this will not
		 * be significant */
		const auto y = std::lower_bound(sccs1.begin(),sccs1.end(),x.first,
			[](const auto& a, const auto& b) { return a.first < b; });
		if( !(y < sccs1.end()) || y->first != x.first ) {
			fprintf(stderr,"ID %u not found in the first dataset!\n",x.first);
			break;
		}
		if(x.second != sccid2) {
			/* new SCC, nothing to compare yet
			 * note: this should ensure that y->second is a new unique sccid,
			 * but currently does not do that
			 * the solution for that is to run the whole program both ways */
			sccid1 = y->second;
			sccid2 = x.second;
		}
		else if(y->second != sccid1) {
			fprintf(stderr,"Mismatch for address %u!\n",x.first);
			break;
		}
	}
	
	return 0;
}

