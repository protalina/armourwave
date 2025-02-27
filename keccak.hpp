#pragma once
#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <utility>

// Keccak-p[1600, 24] permutation
namespace keccak {

// Logarithmic base 2 of bit width of lane i.e. log2(LANE_BW)
static constexpr size_t L = 6;

// Bit width of each lane of Keccak-p[1600, 24] state
static constexpr size_t LANE_BW = 1ul << L;

// # -of lanes ( each of 64 -bit width ) in Keccak-p[1600, 24] state
static constexpr size_t LANE_CNT = 1600 / LANE_BW;

// Keccak-p[b, nr] permutation to be applied `nr` ( = 24 ) rounds
// s.t. b = 1600, w = b/ 25, l = log2(w), nr = 12 + 2l
static constexpr size_t ROUNDS = 12 + 2 * L;

// Leftwards circular rotation offset of 25 lanes ( each lane is 64 -bit wide )
// of state array, as provided in table 2 below algorithm 2 in section 3.2.2 of
// https://dx.doi.org/10.6028/NIST.FIPS.202
//
// Note, following offsets are obtained by performing % 64 ( bit width of lane )
// on offsets provided in above mentioned link
static constexpr size_t ROT[LANE_CNT]{
  0 % LANE_BW,   1 % LANE_BW,   190 % LANE_BW, 28 % LANE_BW,  91 % LANE_BW,
  36 % LANE_BW,  300 % LANE_BW, 6 % LANE_BW,   55 % LANE_BW,  276 % LANE_BW,
  3 % LANE_BW,   10 % LANE_BW,  171 % LANE_BW, 153 % LANE_BW, 231 % LANE_BW,
  105 % LANE_BW, 45 % LANE_BW,  15 % LANE_BW,  21 % LANE_BW,  136 % LANE_BW,
  210 % LANE_BW, 66 % LANE_BW,  253 % LANE_BW, 120 % LANE_BW, 78 % LANE_BW
};

// Precomputed table used for looking up source index during application of π
// step mapping function on keccak-[1600, 24] state
//
// print('to <= from')
// for y in range(5):
//    for x in range(5):
//        print(f'{y * 5 + x} <= {x * 5 + (x + 3 * y) % 5}')
//
// Table generated using above Python code snippet. See section 3.2.3 of the
// specification https://dx.doi.org/10.6028/NIST.FIPS.202
static constexpr size_t PERM[LANE_CNT]{ 0,  6,  12, 18, 24, 3,  9, 10, 16,
                                        22, 1,  7,  13, 19, 20, 4, 5,  11,
                                        17, 23, 2,  8,  14, 15, 21 };

// Computes single bit of Keccak-p[1600, 24] round constant ( at compile-time ),
// using binary LFSR, defined by primitive polynomial x^8 + x^6 + x^5 + x^4 + 1
//
// See algorithm 5 in section 3.2.5 of http://dx.doi.org/10.6028/NIST.FIPS.202
//
// Taken from
// https://github.com/itzmeanjan/elephant/blob/2a21c7e/include/keccak.hpp#L24-L59
consteval static bool
rc(const size_t t)
{
  // step 1 of algorithm 5
  if (t % 255 == 0) {
    return 1;
  }

  // step 2 of algorithm 5
  //
  // note, step 3.a of algorithm 5 is also being
  // executed in this statement ( for first iteration, with i = 1 ) !
  uint16_t r = 0b10000000;

  // step 3 of algorithm 5
  for (size_t i = 1; i <= t % 255; i++) {
    const uint16_t b0 = r & 1;

    r = (r & 0b011111111) ^ ((((r >> 8) & 1) ^ b0) << 8);
    r = (r & 0b111101111) ^ ((((r >> 4) & 1) ^ b0) << 4);
    r = (r & 0b111110111) ^ ((((r >> 3) & 1) ^ b0) << 3);
    r = (r & 0b111111011) ^ ((((r >> 2) & 1) ^ b0) << 2);

    // step 3.f of algorithm 5
    //
    // note, this statement also executes step 3.a for upcoming
    // iterations ( i.e. when i > 1 )
    r >>= 1;
  }

  return static_cast<bool>((r >> 7) & 1);
}

// Computes 64 -bit round constant ( at compile-time ), which is XOR-ed into
// very first lane ( = lane(0, 0) ) of Keccak-p[1600, 24] permutation state
//
// Taken from
// https://github.com/itzmeanjan/elephant/blob/2a21c7e/include/keccak.hpp#L61-L74
consteval static uint64_t
compute_rc(const size_t r_idx)
{
  uint64_t tmp = 0;

  for (size_t j = 0; j < (L + 1); j++) {
    const size_t boff = (1 << j) - 1;
    tmp |= static_cast<uint64_t>(rc(j + 7 * r_idx)) << boff;
  }

  return tmp;
}

// Compile-time evaluate Keccak-p[1600, 24] round constants.
consteval std::array<uint64_t, LANE_CNT>
compute_rcs()
{
  std::array<uint64_t, LANE_CNT> res;
  for (size_t i = 0; i < LANE_CNT; i++) {
    res[i] = compute_rc(i);
  }

  return res;
}

// Round constants to be XORed with lane (0, 0) of keccak-p[1600, 24]
// permutation state, see section 3.2.5 of
// https://dx.doi.org/10.s6028/NIST.FIPS.202
static constexpr auto RC = compute_rcs();

// Keccak-p[1600, 24] step mapping function θ, see section 3.2.1 of SHA3
// specification https://dx.doi.org/10.6028/NIST.FIPS.202
inline static constexpr void
theta(uint64_t* const state)
{
  uint64_t c[5]{};
  uint64_t d[5];

#if defined __clang__
  // Following
  // https://clang.llvm.org/docs/LanguageExtensions.html#extensions-for-loop-hint-optimizations

#pragma clang loop unroll(enable)
#pragma clang loop vectorize(enable)
#elif defined __GNUG__
  // Following
  // https://gcc.gnu.org/onlinedocs/gcc/Loop-Specific-Pragmas.html#Loop-Specific-Pragmas

#pragma GCC unroll 5
#endif
  for (size_t i = 0; i < 25; i += 5) {
    c[0] ^= state[i + 0];
    c[1] ^= state[i + 1];
    c[2] ^= state[i + 2];
    c[3] ^= state[i + 3];
    c[4] ^= state[i + 4];
  }

  d[0] = c[4] ^ std::rotl(c[1], 1);
  d[1] = c[0] ^ std::rotl(c[2], 1);
  d[2] = c[1] ^ std::rotl(c[3], 1);
  d[3] = c[2] ^ std::rotl(c[4], 1);
  d[4] = c[3] ^ std::rotl(c[0], 1);

#if defined __clang__
  // Following
  // https://clang.llvm.org/docs/LanguageExtensions.html#extensions-for-loop-hint-optimizations

#pragma clang loop unroll(enable)
#pragma clang loop vectorize(enable)
#elif defined __GNUG__
  // Following
  // https://gcc.gnu.org/onlinedocs/gcc/Loop-Specific-Pragmas.html#Loop-Specific-Pragmas

#pragma GCC unroll 5
#endif
  for (size_t i = 0; i < 25; i += 5) {
    state[i + 0] ^= d[0];
    state[i + 1] ^= d[1];
    state[i + 2] ^= d[2];
    state[i + 3] ^= d[3];
    state[i + 4] ^= d[4];
  }
}

// Keccak-p[1600, 24] step mapping function ρ, see section 3.2.2 of SHA3
// specification https://dx.doi.org/10.6028/NIST.FIPS.202
inline static constexpr void
rho(uint64_t* const state)
{
#if defined __clang__
  // Following
  // https://clang.llvm.org/docs/LanguageExtensions.html#extensions-for-loop-hint-optimizations

#pragma clang loop unroll(enable)
#pragma clang loop vectorize(enable)
#elif defined __GNUG__
  // Following
  // https://gcc.gnu.org/onlinedocs/gcc/Loop-Specific-Pragmas.html#Loop-Specific-Pragmas

#pragma GCC unroll 25
#endif
  for (size_t i = 0; i < 25; i++) {
    state[i] = std::rotl(state[i], ROT[i]);
  }
}

// Keccak-p[1600, 24] step mapping function π, see section 3.2.3 of SHA3
// specification https://dx.doi.org/10.6028/NIST.FIPS.202
inline static constexpr void
pi(const uint64_t* const __restrict istate, // input permutation state
   uint64_t* const __restrict ostate        // output permutation state
)
{
#if defined __clang__
  // Following
  // https://clang.llvm.org/docs/LanguageExtensions.html#extensions-for-loop-hint-optimizations

#pragma clang loop unroll(enable)
#pragma clang loop vectorize(enable)
#elif defined __GNUG__
  // Following
  // https://gcc.gnu.org/onlinedocs/gcc/Loop-Specific-Pragmas.html#Loop-Specific-Pragmas

#pragma GCC unroll 25
#endif
  for (size_t i = 0; i < 25; i++) {
    ostate[i] = istate[PERM[i]];
  }
}

// Keccak-p[1600, 24] step mapping function χ, see section 3.2.4 of SHA3
// specification https://dx.doi.org/10.6028/NIST.FIPS.202
inline static constexpr void
chi(uint64_t* const state)
{
#if defined __clang__
  // Following
  // https://clang.llvm.org/docs/LanguageExtensions.html#extensions-for-loop-hint-optimizations

#pragma clang loop unroll(enable)
#pragma clang loop vectorize(enable)
#elif defined __GNUG__
  // Following
  // https://gcc.gnu.org/onlinedocs/gcc/Loop-Specific-Pragmas.html#Loop-Specific-Pragmas

#pragma GCC unroll 5
#endif
  for (size_t i = 0; i < 5; i++) {
    const size_t ix5 = i * 5;

    const uint64_t t0 = state[ix5 + 0];
    const uint64_t t1 = state[ix5 + 1];

    state[ix5 + 0] ^= (~t1 & state[ix5 + 2]);
    state[ix5 + 1] ^= (~state[ix5 + 2] & state[ix5 + 3]);
    state[ix5 + 2] ^= (~state[ix5 + 3] & state[ix5 + 4]);
    state[ix5 + 3] ^= (~state[ix5 + 4] & t0);
    state[ix5 + 4] ^= (~t0 & t1);
  }
}

// Keccak-p[1600, 24] step mapping function ι, see section 3.2.5 of SHA3
// specification https://dx.doi.org/10.6028/NIST.FIPS.202
inline static constexpr void
iota(uint64_t* const state, const size_t ridx)
{
  state[0] ^= RC[ridx];
}

// Keccak-p[1600, 24] round function, which applies all five step mapping
// functions in order, updates state array. Note this implementation of round
// function applies two consecutive rounds in a single call i.e. if you invoke
// it to apply round `i` - it first applies round `i` and then round `i+1`.
//
// See section 3.3 of https://dx.doi.org/10.6028/NIST.FIPS.202
inline static constexpr void
roundx2(uint64_t* const state, const size_t ridx)
{
  uint64_t tmp[LANE_CNT]{};

  // Applying round `ridx`
  theta(state);
  rho(state);
  pi(state, tmp);
  chi(tmp);
  iota(tmp, ridx);

  // Applying round `ridx + 1`
  theta(tmp);
  rho(tmp);
  pi(tmp, state);
  chi(state);
  iota(state, ridx + 1);
}

// Keccak-p[1600, 24] permutation, applying 24 rounds of permutation
// on state of dimension 5 x 5 x 64 ( = 1600 ) -bits, using algorithm 7
// defined in section 3.3 of SHA3 specification
// https://dx.doi.org/10.6028/NIST.FIPS.202
inline constexpr void
permute(uint64_t state[LANE_CNT])
{
  for (size_t i = 0; i < ROUNDS; i += 2) {
    roundx2(state, i);
  }
}

}
