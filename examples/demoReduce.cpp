/*! 
 * @file
 * demo for reduce.
 *
 * Also see `demoReduceAll`.
 *
 * For demonstration the pipelines are just built.
 * Replace .build() with .run() and add .dump() in any unit to check the rows.
 * */
#include <iostream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

#include <ezl.hpp>
#include <ezl/algorithms/io.hpp>
#include <ezl/algorithms/fromFile.hpp>
#include <ezl/algorithms/reduces.hpp>

auto f(long res, char ch, int n, float f) {
  return res + 1;
}

void demoReduce() {
  using std::string;
  using std::tuple;
  using std::vector;

  const string inFile = "data/fromFileTests/test1.txt";

  vector<tuple<int, char, float>> inp;
  inp.emplace_back(2, 'c', 1.F);
  inp.emplace_back(2, 'a', 2.F);
  inp.emplace_back(4, 'a', 3.F);
  inp.emplace_back(4, 'c', 4.F);

  auto pipe1 = ezl::rise(ezl::fromMem(inp).split()).build();

  // output cols are key, result of the UDF.
  // cols can be selected in any order by specifying indices in cols<...>()
  // Alternatively, cols can be dropped by specifying indices in dropCols<...>()
  ezl::flow(pipe1)
    .reduce<2>(f, 0L).cols<2, 1>()
    .filter([](long, char ch) { 
      return true; 
    })
    .build();

  // With ordered(), the reduction do not wait till end of data to flush the
  // results of a key. It can be used if we know that data coming to a
  // reduce is ordered. The reduction is essentially done for one key at a
  // time and the resulting row is flushed as soon as any different key appears.
  // In this way the output remains ordered as well. Although, the ordered expr.
  // does not affect results, it increases speed and sets the result in same 
  // order as input.
  // The ordered expression in fromFile makes sure that all the contiguos rows
  // with same value of certain selected columns in a input file are read by
  // the same process in a multi-process run.  
  // see `demoFromFile` for more on this.
  ezl::rise(ezl::fromFile<string, int, float>(inFile)
                          .cols({"name", "num", "score"})
                          .ordered<1>())
    .reduce<1>(ezl::sum(), 0, 0.F) // sums the value cols.
      .ordered()
      .inprocess()
    .filter([](string, int, float) { return true; })
    .build();

  // The example shows a useful idiom.
  // an inprocess reduce followed by another reduce to accumulate the results
  // of various inprocess reductions. 
  // This make parallism much more effective compared to only one reduce.
  // The prll expression makes the resulting count available in all processes,
  // finally returned in the variable iSum. See `demoPrll` for more on this.
  // It is slightly less communication cost if we uncomment the filter.
  auto grpCounts = ezl::flow(pipe1)
                    .reduce<1>(ezl::count(), 0LL) // counts in process rows.
                      .inprocess()
                    .reduce<1>(ezl::sum(), 0LL) // sums the counts
                      .prll(1., ezl::llmode::dupe | ezl::llmode::task)
                    .get();

  // can return vector to return multiple rows.
  // vector<tuple<...>> to return multiple rows of multiple cols.
  // tuple<vector> to return a vector as a column rather than multiple rows.
  // The result parameter can be a reference type, which can be modified and
  // its reference is returned. This eliminates creation and copy for big sized
  // objects like vector. The key and value params can not be ref, however it
  // is good to have them as const-ref. If not returning reference then result
  // also can only be const-ref not ref.
  ezl::flow(pipe1).reduce<ezl::key<2>, ezl::val<3, 3>>(
      [](vector<float>& res, char c, float x, float y) -> vector<float>& {
        res.push_back(x);
        res.push_back(y);
        return res;
      }, vector<float> {})
      .build();

  // running sum with scan property of reduce
  // the result at every input element is also passed to the next unit
  ezl::flow(pipe1)
    .reduce<ezl::key<>, ezl::val<1>>(std::plus<int>(), 0).scan()
    .build();

  // reduce wrap function and everyColFns
  ezl::flow(pipe1)
    .reduce<2>(everyColFns(ezl::wrapBiFnReduce(std::plus<int>())), 0, 0.0).dump()
    .build();

  // reduce wrap predicate and perColFns
  ezl::flow(pipe1)
    .reduce<2>(perColFns(ezl::wrapPredReduce(std::greater<int>()), ezl::sum()), 0, 0.0)
    .run();

  // UDF params can be key, value, result column types, or their const-refs
  // tuple of key, tuple of value, tuple of result column types or const-refs,
  // const-ref of tuple of const-ref of key, value, result types.
  // It is good to care about const-ref if the size of the object is big.  
}

int main(int argc, char *argv[]) {
  // boost::mpi::environment env(argc, argv, false);
  ezl::Env env{argc, argv, false};
  try {
    demoReduce();
  } catch (const std::exception& ex) {
    std::cerr<<"error: "<<ex.what()<<'\n';
    env.abort(1);  
  } catch (...) {
    std::cerr<<"unknown exception\n";
    env.abort(2);  
  }
  return 0;
}
