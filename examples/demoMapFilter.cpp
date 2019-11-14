/*! 
 * @file
 * demo for map and filter.
 *
 * For demonstration the pipelines are not built or run.
 * Add .run() at the end of a flow and add .dump() in a unit to check the rows.
 * */
#include <array>
#include <iostream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

#include <ezl.hpp>
#include <ezl/algorithms/io.hpp>
#include <ezl/algorithms/predicates.hpp>
#include <ezl/algorithms/maps.hpp>

class Op {
public:
  auto operator() (std::string s) {
    return std::make_tuple(s.size(), index);
  }
private:
  long index = 0; 
};

// returning zero to multiple rows can be done by simply
// returning a vector of values.
// Valid for all units inclding reduce and reduceAll.
auto f(int x) {
  return std::vector<int>{1, x};
}

// make_tuple to explicitly return a vector as a column
// rather than multiple rows.
// Valid for all units inclding reduce and reduceAll.
auto fvec(int x) {
  return std::make_tuple(std::vector<int>{1, x});
}

void demoMapFilter() {
  using std::tuple;
  using std::vector;
  using std::array;
  using std::make_tuple;
  using std::to_string;

  // for more on rise with fromMem see `demoIO.cpp`
  // returns row(s) of type int, char and float
  auto pipe1 = ezl::rise(ezl::fromMem({make_tuple(2, 'c', 1.F)})).build();

  // for more on column selection see `demoColumns`
  // filter at the end is to show columns at the end.
  ezl::flow(pipe1)
    .map<1, 2>([](auto num, auto ch) { // lambda with auto as UDF
      return (to_string(num)); 
    })                                 // result is appended to input cols
    .map<4>(Op()).colsTransform()      
    .filter([](int, char, float, size_t, long) {
      return true;
    });

  ezl::flow(pipe1)
    .map<1>(f)  // simple function can be used as UDF
    .filter(ezl::tautology())  // always returns true
    .filter([](tuple<int, char, float, int>) {
      return true;
    });

  // params can be column value types, const-ref of column types
  // tuple of either of column types or const-ref of column types,
  // const-ref of the tuple type.
  // It is good to care about const-ref if the size of the object is big.  
  ezl::flow(pipe1)
    .map<1>(fvec)  // returning vector as a column
    .filter([](int, char, float, const vector<int>&) {
      return true;
    });

  ezl::flow(pipe1)
    .map<1>([](const int& x) { 
        return vector<tuple<int, char>>{make_tuple(x, 'c')};
    }).colsResult()
    .filter<1>(ezl::gt(2)) // row passes if 1st col is > 2
    .filter(ezl::eq(2, 'c')) // satisfies if 1st col == 2 && 2nd col == 'c'
    .filter<2>(ezl::lt('d'))
    .filter<1>(ezl::gt(2) || ezl::lt(0)) // 1st col >2 || < 0
    .filter(!ezl::eq<1>(2) || ezl::eq<2>('c')); // 1st != 2 || 2nd == 'c'

  // mergeAr merges, N cols of type T to one col of type std::array<T, N>
  // it can be used to merge one array and N cols or two arrays of type T
  ezl::rise(ezl::fromMem({array<int, 2>{{1, 2}}, array<int, 2>{{3, 4}}}))
    .map(ezl::explodeAr()).colsTransform() // explodes an array to separate cols
    .map(ezl::serialNumber(1))             // appends serial number to rows 
    .filter([](const tuple<const int&, const int&, const int&>&) {return true;})
    .map(ezl::mergeAr()).colsTransform()   // merges three int cols to an array
    .map(ezl::explodeAr()).colsTransform()
    .map<1,2>(ezl::mergeAr()).colsTransform() // merges just two cols
    .filter([](const array<int, 2>&, const int&) {return true;})
    .map(ezl::mergeAr()).colsTransform()  // merges an array and int to one col
    .filter(ezl::gt<1>(2));               // row passes if 1st col of array > 2
}

int main(int argc, char *argv[]) {
  // boost::mpi::environment env(argc, argv, false);
  ezl::Env env{argc, argv, false};
  try {
    demoMapFilter();
  } catch (const std::exception& ex) {
    std::cerr<<"error: "<<ex.what()<<'\n';
    env.abort(1);  
  } catch (...) {
    std::cerr<<"unknown exception\n";
    env.abort(2);  
  }
  return 0;
}
