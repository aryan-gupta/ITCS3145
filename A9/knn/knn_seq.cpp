#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <tuple>
#include <cstdint>
#include <cassert>
#include <cmath>
#include <algorithm>
#include <map>

typedef int classes;
typedef float feature;

void parsefiledb(std::istream& inputstream,
		 std::vector<std::vector<feature>>& features,
		 std::vector<classes>& cl) {

  //the file is a set of record, one record per line.
  while (inputstream.good()) {
    std::string line;
    std::getline(inputstream, line);

    if (inputstream.good()) {
      //each line represents an object as a comma separated set of features. the last feature is an integer classID
      std::stringstream linestream (line);
      std::vector<feature> featurevector;
      
      while (linestream.good()) {
	std::string feat;
	std::getline(linestream, feat, ',');
	if (linestream.good()||linestream.eof()) {
	  float featval = std::atof(feat.c_str());
	  featurevector.push_back(featval);
	}
      }
      //the last features is actually the class.
      classes c = featurevector.back();
      featurevector.pop_back();

      //record object
      features.push_back(featurevector);
      cl.push_back(c);
    }
  }
}

void parsefilequery(std::istream& inputstream,
		    std::vector<std::vector<feature>>& features) {

  //the file is a set of record, one record per line.
  while (inputstream.good()) {
    std::string line;
    std::getline(inputstream, line);

    if (inputstream.good()) {
      //each line represents an object as a comma separated set of features.
      std::stringstream linestream (line);
      std::vector<feature> featurevector;
      
      while (linestream.good()) {
	std::string feat;
	std::getline(linestream, feat, ',');
	if (linestream.good()||linestream.eof()) {
	  float featval = std::atof(feat.c_str());
	  featurevector.push_back(featval);
	}
      }

      //record object
      features.push_back(featurevector);
    }
  }
}


double distance (feature const * a,
		 feature const * b,
		 long size) {
  double d = 0.;
  for (uint32_t idx = 0; idx < size; ++idx) {
    d += std::pow(a[idx] - b[idx], 2);
  }
  return d;
}


double distance (const std::vector<feature>& a, const std::vector<feature>& b) {
  assert(a.size() == b.size());
  return distance(&a[0], &b[0], a.size());
}

std::vector<uint32_t>  knn(const std::vector<std::vector<feature>>& features,
			   const std::vector<feature>& query,
			   uint32_t k) {
  
  std::vector<std::pair<uint32_t, double>> distances;
  for (uint32_t i = 0; i< features.size(); ++i) {
    distances.push_back(std::make_pair(i , distance(query, features[i])));
  }

  std::sort(distances.begin(), distances.end(), [](std::pair<uint32_t, double> a, std::pair<uint32_t, double> b) {
      return a.second < b.second;
    });

  //return the k nearest distance
  std::vector<uint32_t> neighbors;
  for (uint32_t i = 0; i< k; ++i) {
    neighbors.push_back(distances[i].first);
  }
  
  return neighbors;
}

uint32_t vote(const std::vector<std::vector<feature>>& features,
	      const std::vector<classes>& cl,
	      std::vector<uint32_t> neighbors) {

  std::map<classes, uint32_t> count;
  
  for (auto n : neighbors) {
    count[cl[n]] ++;
  }
  
  //guess class based on votes
  classes maxcl;
  uint32_t maxcount = 0;
  for (auto &pai: count) {
    //std::cout<<pai.first<<" "<<pai.second<<std::endl;
    if (pai.second > maxcount) {
      maxcl = pai.first;
      maxcount = pai.second;
    }
  }
  return maxcl;
}

int main (int argc, char*argv[]) {

  if (argc < 4) {
    std::cerr<<"usage: "<<argv[0]<<" <databasefile> <queryfile> <k>"<<std::endl;
    return -1;
  }
  
  std::vector<std::vector<feature>> features;
  std::vector<classes> cl;  

  std::vector<std::vector<feature>> query;
  
  {
    std::ifstream dbfile(argv[1]);
    parsefiledb(dbfile, features, cl);

    std::ifstream queryfile(argv[2]);
    
    parsefilequery(queryfile, query);
  }

  int k = std::atoi(argv[3]);

  for (uint32_t i = 0; i<query.size(); ++i) {
    std::vector<uint32_t> neighbor = knn(features, query[i], k);

    uint32_t guess = vote(features, cl, neighbor);
    
    std::cout<<"query "<<i<<" is guessed to be in "<<guess<<std::endl;
  }

  return 0;
}
