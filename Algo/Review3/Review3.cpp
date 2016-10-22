#include <iostream>
#include <chrono>
#include <random>
#include <vector>
#include <cmath>
#include <algorithm>
#include <list>
#include <string>
#include <climits>

static constexpr int kKeyShift = -1000000001;

class HashFunction {
 public:
  HashFunction();
  HashFunction(uint64_t multiplier, uint64_t summand,
               uint64_t prime_modulo, uint64_t modulo);

  uint64_t operator()(uint64_t value) const;

 private:
  uint64_t multiplier_;
  uint64_t summand_;
  uint64_t prime_modulo_;
  uint64_t modulo_;
};

class UniversalHashGenerator {
 public:
  UniversalHashGenerator();
  explicit UniversalHashGenerator(uint64_t prime_modulo);

  HashFunction Generate(int hash_table_size);

 private:
  uint64_t prime_modulo_;
  std::mt19937_64 generator_;
  std::uniform_int_distribution<int64_t> multiplier_distribution_;
  std::uniform_int_distribution<int64_t> summand_distribution_;
};

using BucketData = std::vector<int>;

int64_t SumOfSquares(const std::vector<int>& numbers) {
  int64_t size_sum = 0;
  for (const int64_t subtable_size : numbers) {
    size_sum += subtable_size * subtable_size;
  }
  return size_sum;
}

class IPerfectHashTable {
 public:
  IPerfectHashTable();

  virtual void Initialize(const std::vector<int>& numbers) = 0;
  bool Contains(int number) const;

 protected:
  int PositionInTable(int key) const;
  int GetSize() const;
  std::vector<BucketData> MakeChains(const std::vector<int>& numbers) const;
  std::vector<int> ComputeChainSizes(const std::vector<int>& numbers) const;
  void ChooseGoodHash(const std::vector<int>& numbers);

  virtual bool IsHashOk(const std::vector<int>& numbers) const = 0;
  virtual int GetSameHashKey(int key) const = 0;

  static constexpr uint64_t kPrimeModulo = (1ll << 31) - 1;
  static constexpr int kNoKey = 0;

  HashFunction hash_;
  int size_;
};

constexpr int IPerfectHashTable::kNoKey;

class FixedSet;

class PolynomialHashTable : public IPerfectHashTable {
 public:
  friend FixedSet;
  PolynomialHashTable();
  void Initialize(const std::vector<int>& numbers) override;

 protected:
  int GetSameHashKey(int key) const override;

 private:
  bool IsHashOk(const std::vector<int>& numbers) const override;
  void MakeTable(const std::vector<int>& numbers);
  std::vector<int> table_;
};

class FixedSet : public IPerfectHashTable {
 public:
  FixedSet();
  void Initialize(const std::vector<int>& numbers) override;

 private:
  std::vector<PolynomialHashTable> subtables_;

  bool IsHashOk(const std::vector<int>& numbers) const override;
  int GetSameHashKey(int key) const override;

  void MakeSubtables(const std::vector<BucketData>& buckets);

  static constexpr int kSizeCoefficient = 3;
};

std::vector<bool> ProcessRequests(const FixedSet& fixed_set,
                                  const std::vector<int>& queries);

std::vector<int> ReadShiftedIntegers(std::istream& istream = std::cin);

void OutputResponses(const std::vector<bool>& responses,
                     std::ostream& ostream = std::cout);

int main() {
  std::ios_base::sync_with_stdio(false);
  const std::vector<int> numbers_to_store = ReadShiftedIntegers();
  const std::vector<int> queries = ReadShiftedIntegers();

  FixedSet fixed_set;
  fixed_set.Initialize(numbers_to_store);
  OutputResponses(ProcessRequests(fixed_set, queries));
  return 0;
}

FixedSet::FixedSet() { }

void FixedSet::Initialize(const std::vector<int>& numbers) {
  size_ = numbers.size();

  ChooseGoodHash(numbers);
  MakeSubtables(MakeChains(numbers));
}

void FixedSet::MakeSubtables(const std::vector<BucketData>& buckets) {
  subtables_.reserve(size_);
  for (const BucketData& bucket : buckets) {
    subtables_.emplace_back();
    subtables_.back().Initialize(bucket);
  }
}

void IPerfectHashTable::ChooseGoodHash(const std::vector<int>& numbers) {
  auto generator_ = UniversalHashGenerator(kPrimeModulo);
  do {
    hash_ = generator_.Generate(size_);
  } while (!IsHashOk(numbers));
}

bool FixedSet::IsHashOk(const std::vector<int>& numbers) const {
  return SumOfSquares(ComputeChainSizes(numbers)) <= kSizeCoefficient * size_;
}

IPerfectHashTable::IPerfectHashTable() {}

bool IPerfectHashTable::Contains(int number) const {
  return number == GetSameHashKey(number);
}

int IPerfectHashTable::PositionInTable(int key) const {
  return hash_(key);
}

int FixedSet::GetSameHashKey(int key) const {
  return subtables_[PositionInTable(key)].GetSameHashKey(key);
}

PolynomialHashTable::PolynomialHashTable() { }

void PolynomialHashTable::Initialize(const std::vector<int>& numbers) {
  size_ = numbers.size() * numbers.size();
  if (size_ == 0) {
    return;
  }

  ChooseGoodHash(numbers);
  MakeTable(numbers);
}

bool PolynomialHashTable::IsHashOk(const std::vector<int>& numbers) const {
  std::vector<int> bucket_sizes;
  bucket_sizes = ComputeChainSizes(numbers);
  return *std::max_element(bucket_sizes.begin(), bucket_sizes.end()) <= 1;
}

void PolynomialHashTable::MakeTable(const std::vector<int>& numbers) {
  table_.reserve(size_);
  for (const auto& keys : MakeChains(numbers)) {
    table_.push_back(keys.empty() ? kNoKey : keys.back());
  }
}

int PolynomialHashTable::GetSameHashKey(int key) const {
  return size_ == 0 ? kNoKey : table_[PositionInTable(key)];
}

int IPerfectHashTable::GetSize() const {
  return size_;
}

std::vector<BucketData> IPerfectHashTable::MakeChains(
    const std::vector<int>& numbers) const {
  std::vector<BucketData> buckets(size_);
  for (const auto key : numbers) {
    buckets[hash_(key)].push_back(key);
  }
  return buckets;
}

std::vector<int> IPerfectHashTable::ComputeChainSizes(
    const std::vector<int>& numbers) const {
  std::vector<int> bucket_sizes(size_, 0);
  for (const auto key : numbers) {
    ++bucket_sizes[hash_(key)];
  }
  return bucket_sizes;
}


HashFunction::HashFunction() {}

HashFunction::HashFunction(uint64_t _multiplier, uint64_t _summand,
                           uint64_t _prime_modulo, uint64_t _modulo)
    : multiplier_(_multiplier), summand_(_summand),
      prime_modulo_(_prime_modulo), modulo_(_modulo) {}

uint64_t HashFunction::operator()(uint64_t value) const {
  return ((multiplier_ * value + summand_) % prime_modulo_) % modulo_;
}

UniversalHashGenerator::UniversalHashGenerator()
    : generator_(std::chrono::system_clock::now().time_since_epoch().count()) {}

UniversalHashGenerator::UniversalHashGenerator(uint64_t _prime_modulo)
    : prime_modulo_(_prime_modulo),
      generator_(std::chrono::system_clock::now().time_since_epoch().count()),
      multiplier_distribution_(1, _prime_modulo - 1),
      summand_distribution_(0, _prime_modulo - 1) {}

HashFunction UniversalHashGenerator::Generate(int hash_table_size) {
  return {multiplier_distribution_(generator_),
          summand_distribution_(generator_),
          prime_modulo_,
          hash_table_size};
}

std::vector<int> ReadShiftedIntegers(std::istream& istream) {
  size_t vector_size;
  istream >> vector_size;
  std::vector<int> input_vector(vector_size);
  for (auto& element : input_vector) {
    istream >> element;
    element -= kKeyShift;
  }
  return input_vector;
}

void OutputResponses(const std::vector<bool>& responses,
                     std::ostream& ostream) {
  static const std::string kYesResponse = "Yes";
  static const std::string kNoResponse = "No";

  for (const auto element : responses) {
    ostream << (element ? kYesResponse : kNoResponse) << "\n";
  }
}

std::vector<bool> ProcessRequests(
    const FixedSet& fixed_set,
    const std::vector<int>& queries) {
  std::vector<bool> responses;
  responses.reserve(queries.size());
  for (const auto query : queries) {
    responses.push_back(fixed_set.Contains(query));
  }
  return responses;
}
