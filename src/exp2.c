/* Correctly rounded exp2 function for binary64 values.

Copyright (c) 2021-2022 Paul Zimmermann and Stéphane Glondu, Inria.

This file is part of the CORE-MATH project
(https://core-math.gitlabpages.inria.fr/).

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "math.h" //Media Enhanced Change (MEC): Added
//#include <stdint.h> /* for uint64_t */ //MEC: Now included in math.h
//#include <math.h>   /* for ldexp and isnan */ //MEC: Now included in math.h

/* Add a + b exactly, such that *hi + *lo = a + b.
   Assumes |a| >= |b|.  */
static void
fast_two_sum (double *hi, double *lo, double a, double b)
{
  double e;

  *hi = a + b;
  e = *hi - a; /* exact */
  *lo = b - e; /* exact */
  /* Now hi + lo = a + b exactly for rounding to nearest.
     For directed rounding modes, this is not always true.
     Take for example a = 1, b = 2^-200, and rounding up,
     then hi = 1 + 2^-52, e = 2^-52 (it can be proven that
     e is always exact), and lo = -2^52 + 2^-105, thus
     hi + lo = 1 + 2^-105 <> a + b = 1 + 2^-200.
     A bound on the error is given
     in "Note on FastTwoSum with Directed Roundings"
     by Paul Zimmermann, https://hal.inria.fr/hal-03798376, 2022.
     Theorem 1 says that
     the difference between a+b and hi+lo is bounded by 2u^2|a+b|
     and also by 2u^2|hi|. Here u=2^-53, thus we get:
     |(a+b)-(hi+lo)| <= 2^-105 min(|a+b|,|hi|) */
}

/* h + l <- a * b
   We have h + l = a * b exactly, whatever the rounding mode, when no
   underflow happens (cf Section 4.4 of the Handbook of Floating-Point
   Arithmetic, 2nd edition) */
static void
dekker (double *h, double *l, double a, double b)
{
  *h = a * b;
  *l = fmaDouble(a, b, -*h); //MEC: Use Math Library FMA
}

typedef union { double x; uint64_t n; } d64u64;

/* for -127 <= i <= 127, tab_i[127+i] is a double-double approximation
   of 2^(i/128) */
static const double tab_i[255][2] = {
{ 0x1.0163da9fb3335p-1, 0x1.b61299ab8cdb7p-55 }, /* -127 */
{ 0x1.02c9a3e778061p-1, -0x1.19083535b085dp-57 }, /* -126 */
{ 0x1.04315e86e7f85p-1, -0x1.0a31c1977c96ep-55 }, /* -125 */
{ 0x1.059b0d3158574p-1, 0x1.d73e2a475b465p-56 }, /* -124 */
{ 0x1.0706b29ddf6dep-1, -0x1.c91dfe2b13c27p-56 }, /* -123 */
{ 0x1.0874518759bc8p-1, 0x1.186be4bb284ffp-58 }, /* -122 */
{ 0x1.09e3ecac6f383p-1, 0x1.1487818316136p-55 }, /* -121 */
{ 0x1.0b5586cf9890fp-1, 0x1.8a62e4adc610bp-55 }, /* -120 */
{ 0x1.0cc922b7247f7p-1, 0x1.01edc16e24f71p-55 }, /* -119 */
{ 0x1.0e3ec32d3d1a2p-1, 0x1.03a1727c57b53p-60 }, /* -118 */
{ 0x1.0fb66affed31bp-1, -0x1.b9bedc44ebd7bp-58 }, /* -117 */
{ 0x1.11301d0125b51p-1, -0x1.6c51039449b3ap-55 }, /* -116 */
{ 0x1.12abdc06c31ccp-1, -0x1.1b514b36ca5c7p-59 }, /* -115 */
{ 0x1.1429aaea92dep-1, -0x1.32fbf9af1369ep-55 }, /* -114 */
{ 0x1.15a98c8a58e51p-1, 0x1.2406ab9eeab0ap-56 }, /* -113 */
{ 0x1.172b83c7d517bp-1, -0x1.19041b9d78a76p-56 }, /* -112 */
{ 0x1.18af9388c8deap-1, -0x1.11023d1970f6cp-55 }, /* -111 */
{ 0x1.1a35beb6fcb75p-1, 0x1.e5b4c7b4968e4p-56 }, /* -110 */
{ 0x1.1bbe084045cd4p-1, -0x1.95386352ef607p-55 }, /* -109 */
{ 0x1.1d4873168b9aap-1, 0x1.e016e00a2643cp-55 }, /* -108 */
{ 0x1.1ed5022fcd91dp-1, -0x1.1df98027bb78cp-55 }, /* -107 */
{ 0x1.2063b88628cd6p-1, 0x1.dc775814a8495p-56 }, /* -106 */
{ 0x1.21f49917ddc96p-1, 0x1.2a97e9494a5eep-56 }, /* -105 */
{ 0x1.2387a6e756238p-1, 0x1.9b07eb6c70573p-55 }, /* -104 */
{ 0x1.251ce4fb2a63fp-1, 0x1.ac155bef4f4a4p-56 }, /* -103 */
{ 0x1.26b4565e27cddp-1, 0x1.2bd339940e9d9p-56 }, /* -102 */
{ 0x1.284dfe1f56381p-1, -0x1.a4c3a8c3f0d7ep-55 }, /* -101 */
{ 0x1.29e9df51fdee1p-1, 0x1.612e8afad1255p-56 }, /* -100 */
{ 0x1.2b87fd0dad99p-1, -0x1.10adcd6381aa4p-60 }, /* -99 */
{ 0x1.2d285a6e4030bp-1, 0x1.0024754db41d5p-55 }, /* -98 */
{ 0x1.2ecafa93e2f56p-1, 0x1.1ca0f45d52383p-57 }, /* -97 */
{ 0x1.306fe0a31b715p-1, 0x1.6f46ad23182e4p-56 }, /* -96 */
{ 0x1.32170fc4cd831p-1, 0x1.a9ce78e18047cp-56 }, /* -95 */
{ 0x1.33c08b26416ffp-1, 0x1.32721843659a6p-55 }, /* -94 */
{ 0x1.356c55f929ff1p-1, -0x1.b5cee5c4e4628p-56 }, /* -93 */
{ 0x1.371a7373aa9cbp-1, -0x1.63aeabf42eae2p-55 }, /* -92 */
{ 0x1.38cae6d05d866p-1, -0x1.e958d3c9904bdp-55 }, /* -91 */
{ 0x1.3a7db34e59ff7p-1, -0x1.5e436d661f5e3p-57 }, /* -90 */
{ 0x1.3c32dc313a8e5p-1, -0x1.efff8375d29c3p-55 }, /* -89 */
{ 0x1.3dea64c123422p-1, 0x1.ada0911f09ebcp-56 }, /* -88 */
{ 0x1.3fa4504ac801cp-1, -0x1.7d023f956f9f3p-55 }, /* -87 */
{ 0x1.4160a21f72e2ap-1, -0x1.ef3691c309278p-59 }, /* -86 */
{ 0x1.431f5d950a897p-1, -0x1.1c7dde35f7999p-56 }, /* -85 */
{ 0x1.44e086061892dp-1, 0x1.89b7a04ef80dp-60 }, /* -84 */
{ 0x1.46a41ed1d0057p-1, 0x1.c944bd1648a76p-55 }, /* -83 */
{ 0x1.486a2b5c13cdp-1, 0x1.3c1a3b69062fp-57 }, /* -82 */
{ 0x1.4a32af0d7d3dep-1, 0x1.9cb62f3d1be56p-55 }, /* -81 */
{ 0x1.4bfdad5362a27p-1, 0x1.d4397afec42e2p-57 }, /* -80 */
{ 0x1.4dcb299fddd0dp-1, 0x1.8ecdbbc6a7833p-55 }, /* -79 */
{ 0x1.4f9b2769d2ca7p-1, -0x1.4b309d25957e3p-55 }, /* -78 */
{ 0x1.516daa2cf6642p-1, -0x1.f768569bd93efp-56 }, /* -77 */
{ 0x1.5342b569d4f82p-1, -0x1.07abe1db13cadp-56 }, /* -76 */
{ 0x1.551a4ca5d920fp-1, -0x1.d689cefede59bp-56 }, /* -75 */
{ 0x1.56f4736b527dap-1, 0x1.9bb2c011d93adp-55 }, /* -74 */
{ 0x1.58d12d497c7fdp-1, 0x1.295e15b9a1de8p-56 }, /* -73 */
{ 0x1.5ab07dd485429p-1, 0x1.6324c054647adp-55 }, /* -72 */
{ 0x1.5c9268a5946b7p-1, 0x1.c4b1b816986a2p-61 }, /* -71 */
{ 0x1.5e76f15ad2148p-1, 0x1.ba6f93080e65ep-55 }, /* -70 */
{ 0x1.605e1b976dc09p-1, -0x1.3e2429b56de47p-55 }, /* -69 */
{ 0x1.6247eb03a5585p-1, -0x1.383c17e40b497p-55 }, /* -68 */
{ 0x1.6434634ccc32p-1, -0x1.c483c759d8933p-56 }, /* -67 */
{ 0x1.6623882552225p-1, -0x1.bb60987591c34p-55 }, /* -66 */
{ 0x1.68155d44ca973p-1, 0x1.038ae44f73e65p-58 }, /* -65 */
{ 0x1.6a09e667f3bcdp-1, -0x1.bdd3413b26456p-55 }, /* -64 */
{ 0x1.6c012750bdabfp-1, -0x1.2895667ff0b0dp-57 }, /* -63 */
{ 0x1.6dfb23c651a2fp-1, -0x1.bbe3a683c88abp-58 }, /* -62 */
{ 0x1.6ff7df9519484p-1, -0x1.83c0f25860ef6p-56 }, /* -61 */
{ 0x1.71f75e8ec5f74p-1, -0x1.16e4786887a99p-56 }, /* -60 */
{ 0x1.73f9a48a58174p-1, -0x1.0a8d96c65d53cp-55 }, /* -59 */
{ 0x1.75feb564267c9p-1, -0x1.0245957316dd3p-55 }, /* -58 */
{ 0x1.780694fde5d3fp-1, 0x1.866b80a02162dp-55 }, /* -57 */
{ 0x1.7a11473eb0187p-1, -0x1.41577ee04992fp-56 }, /* -56 */
{ 0x1.7c1ed0130c132p-1, 0x1.f124cd1164dd6p-55 }, /* -55 */
{ 0x1.7e2f336cf4e62p-1, 0x1.05d02ba15797ep-57 }, /* -54 */
{ 0x1.80427543e1a12p-1, -0x1.27c86626d972bp-55 }, /* -53 */
{ 0x1.82589994cce13p-1, -0x1.d4c1dd41532d8p-55 }, /* -52 */
{ 0x1.8471a4623c7adp-1, -0x1.8d684a341cdfbp-56 }, /* -51 */
{ 0x1.868d99b4492edp-1, -0x1.fc6f89bd4f6bap-55 }, /* -50 */
{ 0x1.88ac7d98a6699p-1, 0x1.994c2f37cb53ap-55 }, /* -49 */
{ 0x1.8ace5422aa0dbp-1, 0x1.6e9f156864b27p-55 }, /* -48 */
{ 0x1.8cf3216b5448cp-1, -0x1.0d55e32e9e3aap-57 }, /* -47 */
{ 0x1.8f1ae99157736p-1, 0x1.5cc13a2e3976cp-56 }, /* -46 */
{ 0x1.9145b0b91ffc6p-1, -0x1.dd6792e582524p-55 }, /* -45 */
{ 0x1.93737b0cdc5e5p-1, -0x1.75fc781b57ebcp-58 }, /* -44 */
{ 0x1.95a44cbc8520fp-1, -0x1.64b7c96a5f039p-57 }, /* -43 */
{ 0x1.97d829fde4e5p-1, -0x1.d185b7c1b85d1p-55 }, /* -42 */
{ 0x1.9a0f170ca07bap-1, -0x1.173bd91cee632p-55 }, /* -41 */
{ 0x1.9c49182a3f09p-1, 0x1.c7c46b071f2bep-57 }, /* -40 */
{ 0x1.9e86319e32323p-1, 0x1.824ca78e64c6ep-57 }, /* -39 */
{ 0x1.a0c667b5de565p-1, -0x1.359495d1cd533p-55 }, /* -38 */
{ 0x1.a309bec4a2d33p-1, 0x1.6305c7ddc36abp-55 }, /* -37 */
{ 0x1.a5503b23e255dp-1, -0x1.d2f6edb8d41e1p-55 }, /* -36 */
{ 0x1.a799e1330b358p-1, 0x1.bcb7ecac563c7p-55 }, /* -35 */
{ 0x1.a9e6b5579fdbfp-1, 0x1.0fac90ef7fd31p-55 }, /* -34 */
{ 0x1.ac36bbfd3f37ap-1, -0x1.f9234cae76cdp-56 }, /* -33 */
{ 0x1.ae89f995ad3adp-1, 0x1.7a1cd345dcc81p-55 }, /* -32 */
{ 0x1.b0e07298db666p-1, -0x1.bdef54c80e425p-55 }, /* -31 */
{ 0x1.b33a2b84f15fbp-1, -0x1.2805e3084d708p-58 }, /* -30 */
{ 0x1.b59728de5593ap-1, -0x1.c71dfbbba6de3p-55 }, /* -29 */
{ 0x1.b7f76f2fb5e47p-1, -0x1.5584f7e54ac3bp-57 }, /* -28 */
{ 0x1.ba5b030a1064ap-1, -0x1.efcd30e54292ep-55 }, /* -27 */
{ 0x1.bcc1e904bc1d2p-1, 0x1.23dd07a2d9e84p-56 }, /* -26 */
{ 0x1.bf2c25bd71e09p-1, -0x1.efdca3f6b9c73p-55 }, /* -25 */
{ 0x1.c199bdd85529cp-1, 0x1.11065895048ddp-56 }, /* -24 */
{ 0x1.c40ab5fffd07ap-1, 0x1.b4537e083c60ap-55 }, /* -23 */
{ 0x1.c67f12e57d14bp-1, 0x1.2884dff483cadp-55 }, /* -22 */
{ 0x1.c8f6d9406e7b5p-1, 0x1.1acbc48805c44p-57 }, /* -21 */
{ 0x1.cb720dcef9069p-1, 0x1.503cbd1e949dbp-57 }, /* -20 */
{ 0x1.cdf0b555dc3fap-1, -0x1.dd83b53829d72p-56 }, /* -19 */
{ 0x1.d072d4a07897cp-1, -0x1.cbc3743797a9cp-55 }, /* -18 */
{ 0x1.d2f87080d89f2p-1, -0x1.d487b719d8578p-55 }, /* -17 */
{ 0x1.d5818dcfba487p-1, 0x1.2ed02d75b3707p-56 }, /* -16 */
{ 0x1.d80e316c98398p-1, -0x1.11ec18beddfe8p-55 }, /* -15 */
{ 0x1.da9e603db3285p-1, 0x1.c2300696db532p-55 }, /* -14 */
{ 0x1.dd321f301b46p-1, 0x1.2da5778f018c3p-55 }, /* -13 */
{ 0x1.dfc97337b9b5fp-1, -0x1.1a5cd4f184b5cp-55 }, /* -12 */
{ 0x1.e264614f5a129p-1, -0x1.7b627817a1496p-55 }, /* -11 */
{ 0x1.e502ee78b3ff6p-1, 0x1.39e8980a9cc8fp-56 }, /* -10 */
{ 0x1.e7a51fbc74c83p-1, 0x1.2d522ca0c8de2p-55 }, /* -9 */
{ 0x1.ea4afa2a490dap-1, -0x1.e9c23179c2893p-55 }, /* -8 */
{ 0x1.ecf482d8e67f1p-1, -0x1.c93f3b411ad8cp-55 }, /* -7 */
{ 0x1.efa1bee615a27p-1, 0x1.dc7f486a4b6bp-55 }, /* -6 */
{ 0x1.f252b376bba97p-1, 0x1.3a1a5bf0d8e43p-55 }, /* -5 */
{ 0x1.f50765b6e454p-1, 0x1.9d3e12dd8a18bp-55 }, /* -4 */
{ 0x1.f7bfdad9cbe14p-1, -0x1.dbb12d006350ap-55 }, /* -3 */
{ 0x1.fa7c1819e90d8p-1, 0x1.74853f3a5931ep-56 }, /* -2 */
{ 0x1.fd3c22b8f71f1p-1, 0x1.2eb74966579e7p-58 }, /* -1 */
{ 0x1p+0, 0x0p+0 }, /* 0 */
{ 0x1.0163da9fb3335p+0, 0x1.b61299ab8cdb7p-54 }, /* 1 */
{ 0x1.02c9a3e778061p+0, -0x1.19083535b085dp-56 }, /* 2 */
{ 0x1.04315e86e7f85p+0, -0x1.0a31c1977c96ep-54 }, /* 3 */
{ 0x1.059b0d3158574p+0, 0x1.d73e2a475b465p-55 }, /* 4 */
{ 0x1.0706b29ddf6dep+0, -0x1.c91dfe2b13c27p-55 }, /* 5 */
{ 0x1.0874518759bc8p+0, 0x1.186be4bb284ffp-57 }, /* 6 */
{ 0x1.09e3ecac6f383p+0, 0x1.1487818316136p-54 }, /* 7 */
{ 0x1.0b5586cf9890fp+0, 0x1.8a62e4adc610bp-54 }, /* 8 */
{ 0x1.0cc922b7247f7p+0, 0x1.01edc16e24f71p-54 }, /* 9 */
{ 0x1.0e3ec32d3d1a2p+0, 0x1.03a1727c57b53p-59 }, /* 10 */
{ 0x1.0fb66affed31bp+0, -0x1.b9bedc44ebd7bp-57 }, /* 11 */
{ 0x1.11301d0125b51p+0, -0x1.6c51039449b3ap-54 }, /* 12 */
{ 0x1.12abdc06c31ccp+0, -0x1.1b514b36ca5c7p-58 }, /* 13 */
{ 0x1.1429aaea92dep+0, -0x1.32fbf9af1369ep-54 }, /* 14 */
{ 0x1.15a98c8a58e51p+0, 0x1.2406ab9eeab0ap-55 }, /* 15 */
{ 0x1.172b83c7d517bp+0, -0x1.19041b9d78a76p-55 }, /* 16 */
{ 0x1.18af9388c8deap+0, -0x1.11023d1970f6cp-54 }, /* 17 */
{ 0x1.1a35beb6fcb75p+0, 0x1.e5b4c7b4968e4p-55 }, /* 18 */
{ 0x1.1bbe084045cd4p+0, -0x1.95386352ef607p-54 }, /* 19 */
{ 0x1.1d4873168b9aap+0, 0x1.e016e00a2643cp-54 }, /* 20 */
{ 0x1.1ed5022fcd91dp+0, -0x1.1df98027bb78cp-54 }, /* 21 */
{ 0x1.2063b88628cd6p+0, 0x1.dc775814a8495p-55 }, /* 22 */
{ 0x1.21f49917ddc96p+0, 0x1.2a97e9494a5eep-55 }, /* 23 */
{ 0x1.2387a6e756238p+0, 0x1.9b07eb6c70573p-54 }, /* 24 */
{ 0x1.251ce4fb2a63fp+0, 0x1.ac155bef4f4a4p-55 }, /* 25 */
{ 0x1.26b4565e27cddp+0, 0x1.2bd339940e9d9p-55 }, /* 26 */
{ 0x1.284dfe1f56381p+0, -0x1.a4c3a8c3f0d7ep-54 }, /* 27 */
{ 0x1.29e9df51fdee1p+0, 0x1.612e8afad1255p-55 }, /* 28 */
{ 0x1.2b87fd0dad99p+0, -0x1.10adcd6381aa4p-59 }, /* 29 */
{ 0x1.2d285a6e4030bp+0, 0x1.0024754db41d5p-54 }, /* 30 */
{ 0x1.2ecafa93e2f56p+0, 0x1.1ca0f45d52383p-56 }, /* 31 */
{ 0x1.306fe0a31b715p+0, 0x1.6f46ad23182e4p-55 }, /* 32 */
{ 0x1.32170fc4cd831p+0, 0x1.a9ce78e18047cp-55 }, /* 33 */
{ 0x1.33c08b26416ffp+0, 0x1.32721843659a6p-54 }, /* 34 */
{ 0x1.356c55f929ff1p+0, -0x1.b5cee5c4e4628p-55 }, /* 35 */
{ 0x1.371a7373aa9cbp+0, -0x1.63aeabf42eae2p-54 }, /* 36 */
{ 0x1.38cae6d05d866p+0, -0x1.e958d3c9904bdp-54 }, /* 37 */
{ 0x1.3a7db34e59ff7p+0, -0x1.5e436d661f5e3p-56 }, /* 38 */
{ 0x1.3c32dc313a8e5p+0, -0x1.efff8375d29c3p-54 }, /* 39 */
{ 0x1.3dea64c123422p+0, 0x1.ada0911f09ebcp-55 }, /* 40 */
{ 0x1.3fa4504ac801cp+0, -0x1.7d023f956f9f3p-54 }, /* 41 */
{ 0x1.4160a21f72e2ap+0, -0x1.ef3691c309278p-58 }, /* 42 */
{ 0x1.431f5d950a897p+0, -0x1.1c7dde35f7999p-55 }, /* 43 */
{ 0x1.44e086061892dp+0, 0x1.89b7a04ef80dp-59 }, /* 44 */
{ 0x1.46a41ed1d0057p+0, 0x1.c944bd1648a76p-54 }, /* 45 */
{ 0x1.486a2b5c13cdp+0, 0x1.3c1a3b69062fp-56 }, /* 46 */
{ 0x1.4a32af0d7d3dep+0, 0x1.9cb62f3d1be56p-54 }, /* 47 */
{ 0x1.4bfdad5362a27p+0, 0x1.d4397afec42e2p-56 }, /* 48 */
{ 0x1.4dcb299fddd0dp+0, 0x1.8ecdbbc6a7833p-54 }, /* 49 */
{ 0x1.4f9b2769d2ca7p+0, -0x1.4b309d25957e3p-54 }, /* 50 */
{ 0x1.516daa2cf6642p+0, -0x1.f768569bd93efp-55 }, /* 51 */
{ 0x1.5342b569d4f82p+0, -0x1.07abe1db13cadp-55 }, /* 52 */
{ 0x1.551a4ca5d920fp+0, -0x1.d689cefede59bp-55 }, /* 53 */
{ 0x1.56f4736b527dap+0, 0x1.9bb2c011d93adp-54 }, /* 54 */
{ 0x1.58d12d497c7fdp+0, 0x1.295e15b9a1de8p-55 }, /* 55 */
{ 0x1.5ab07dd485429p+0, 0x1.6324c054647adp-54 }, /* 56 */
{ 0x1.5c9268a5946b7p+0, 0x1.c4b1b816986a2p-60 }, /* 57 */
{ 0x1.5e76f15ad2148p+0, 0x1.ba6f93080e65ep-54 }, /* 58 */
{ 0x1.605e1b976dc09p+0, -0x1.3e2429b56de47p-54 }, /* 59 */
{ 0x1.6247eb03a5585p+0, -0x1.383c17e40b497p-54 }, /* 60 */
{ 0x1.6434634ccc32p+0, -0x1.c483c759d8933p-55 }, /* 61 */
{ 0x1.6623882552225p+0, -0x1.bb60987591c34p-54 }, /* 62 */
{ 0x1.68155d44ca973p+0, 0x1.038ae44f73e65p-57 }, /* 63 */
{ 0x1.6a09e667f3bcdp+0, -0x1.bdd3413b26456p-54 }, /* 64 */
{ 0x1.6c012750bdabfp+0, -0x1.2895667ff0b0dp-56 }, /* 65 */
{ 0x1.6dfb23c651a2fp+0, -0x1.bbe3a683c88abp-57 }, /* 66 */
{ 0x1.6ff7df9519484p+0, -0x1.83c0f25860ef6p-55 }, /* 67 */
{ 0x1.71f75e8ec5f74p+0, -0x1.16e4786887a99p-55 }, /* 68 */
{ 0x1.73f9a48a58174p+0, -0x1.0a8d96c65d53cp-54 }, /* 69 */
{ 0x1.75feb564267c9p+0, -0x1.0245957316dd3p-54 }, /* 70 */
{ 0x1.780694fde5d3fp+0, 0x1.866b80a02162dp-54 }, /* 71 */
{ 0x1.7a11473eb0187p+0, -0x1.41577ee04992fp-55 }, /* 72 */
{ 0x1.7c1ed0130c132p+0, 0x1.f124cd1164dd6p-54 }, /* 73 */
{ 0x1.7e2f336cf4e62p+0, 0x1.05d02ba15797ep-56 }, /* 74 */
{ 0x1.80427543e1a12p+0, -0x1.27c86626d972bp-54 }, /* 75 */
{ 0x1.82589994cce13p+0, -0x1.d4c1dd41532d8p-54 }, /* 76 */
{ 0x1.8471a4623c7adp+0, -0x1.8d684a341cdfbp-55 }, /* 77 */
{ 0x1.868d99b4492edp+0, -0x1.fc6f89bd4f6bap-54 }, /* 78 */
{ 0x1.88ac7d98a6699p+0, 0x1.994c2f37cb53ap-54 }, /* 79 */
{ 0x1.8ace5422aa0dbp+0, 0x1.6e9f156864b27p-54 }, /* 80 */
{ 0x1.8cf3216b5448cp+0, -0x1.0d55e32e9e3aap-56 }, /* 81 */
{ 0x1.8f1ae99157736p+0, 0x1.5cc13a2e3976cp-55 }, /* 82 */
{ 0x1.9145b0b91ffc6p+0, -0x1.dd6792e582524p-54 }, /* 83 */
{ 0x1.93737b0cdc5e5p+0, -0x1.75fc781b57ebcp-57 }, /* 84 */
{ 0x1.95a44cbc8520fp+0, -0x1.64b7c96a5f039p-56 }, /* 85 */
{ 0x1.97d829fde4e5p+0, -0x1.d185b7c1b85d1p-54 }, /* 86 */
{ 0x1.9a0f170ca07bap+0, -0x1.173bd91cee632p-54 }, /* 87 */
{ 0x1.9c49182a3f09p+0, 0x1.c7c46b071f2bep-56 }, /* 88 */
{ 0x1.9e86319e32323p+0, 0x1.824ca78e64c6ep-56 }, /* 89 */
{ 0x1.a0c667b5de565p+0, -0x1.359495d1cd533p-54 }, /* 90 */
{ 0x1.a309bec4a2d33p+0, 0x1.6305c7ddc36abp-54 }, /* 91 */
{ 0x1.a5503b23e255dp+0, -0x1.d2f6edb8d41e1p-54 }, /* 92 */
{ 0x1.a799e1330b358p+0, 0x1.bcb7ecac563c7p-54 }, /* 93 */
{ 0x1.a9e6b5579fdbfp+0, 0x1.0fac90ef7fd31p-54 }, /* 94 */
{ 0x1.ac36bbfd3f37ap+0, -0x1.f9234cae76cdp-55 }, /* 95 */
{ 0x1.ae89f995ad3adp+0, 0x1.7a1cd345dcc81p-54 }, /* 96 */
{ 0x1.b0e07298db666p+0, -0x1.bdef54c80e425p-54 }, /* 97 */
{ 0x1.b33a2b84f15fbp+0, -0x1.2805e3084d708p-57 }, /* 98 */
{ 0x1.b59728de5593ap+0, -0x1.c71dfbbba6de3p-54 }, /* 99 */
{ 0x1.b7f76f2fb5e47p+0, -0x1.5584f7e54ac3bp-56 }, /* 100 */
{ 0x1.ba5b030a1064ap+0, -0x1.efcd30e54292ep-54 }, /* 101 */
{ 0x1.bcc1e904bc1d2p+0, 0x1.23dd07a2d9e84p-55 }, /* 102 */
{ 0x1.bf2c25bd71e09p+0, -0x1.efdca3f6b9c73p-54 }, /* 103 */
{ 0x1.c199bdd85529cp+0, 0x1.11065895048ddp-55 }, /* 104 */
{ 0x1.c40ab5fffd07ap+0, 0x1.b4537e083c60ap-54 }, /* 105 */
{ 0x1.c67f12e57d14bp+0, 0x1.2884dff483cadp-54 }, /* 106 */
{ 0x1.c8f6d9406e7b5p+0, 0x1.1acbc48805c44p-56 }, /* 107 */
{ 0x1.cb720dcef9069p+0, 0x1.503cbd1e949dbp-56 }, /* 108 */
{ 0x1.cdf0b555dc3fap+0, -0x1.dd83b53829d72p-55 }, /* 109 */
{ 0x1.d072d4a07897cp+0, -0x1.cbc3743797a9cp-54 }, /* 110 */
{ 0x1.d2f87080d89f2p+0, -0x1.d487b719d8578p-54 }, /* 111 */
{ 0x1.d5818dcfba487p+0, 0x1.2ed02d75b3707p-55 }, /* 112 */
{ 0x1.d80e316c98398p+0, -0x1.11ec18beddfe8p-54 }, /* 113 */
{ 0x1.da9e603db3285p+0, 0x1.c2300696db532p-54 }, /* 114 */
{ 0x1.dd321f301b46p+0, 0x1.2da5778f018c3p-54 }, /* 115 */
{ 0x1.dfc97337b9b5fp+0, -0x1.1a5cd4f184b5cp-54 }, /* 116 */
{ 0x1.e264614f5a129p+0, -0x1.7b627817a1496p-54 }, /* 117 */
{ 0x1.e502ee78b3ff6p+0, 0x1.39e8980a9cc8fp-55 }, /* 118 */
{ 0x1.e7a51fbc74c83p+0, 0x1.2d522ca0c8de2p-54 }, /* 119 */
{ 0x1.ea4afa2a490dap+0, -0x1.e9c23179c2893p-54 }, /* 120 */
{ 0x1.ecf482d8e67f1p+0, -0x1.c93f3b411ad8cp-54 }, /* 121 */
{ 0x1.efa1bee615a27p+0, 0x1.dc7f486a4b6bp-54 }, /* 122 */
{ 0x1.f252b376bba97p+0, 0x1.3a1a5bf0d8e43p-54 }, /* 123 */
{ 0x1.f50765b6e454p+0, 0x1.9d3e12dd8a18bp-54 }, /* 124 */
{ 0x1.f7bfdad9cbe14p+0, -0x1.dbb12d006350ap-54 }, /* 125 */
{ 0x1.fa7c1819e90d8p+0, 0x1.74853f3a5931ep-55 }, /* 126 */
{ 0x1.fd3c22b8f71f1p+0, 0x1.2eb74966579e7p-57 }, /* 127 */
};

static const double xmax = 0x1p1023;

#define MAYBE_UNUSED __attribute__ ((unused))

/* compute a double-double approximation of
   2^x ~ 2^e * 2^(i/128) * 2^h * 2^l
   where -127 <= i <= 127, |h| < 1/128, and |l| < 2^-42.
   We use a degree-9 polynomial to approximate 2^h on [-1/128,1/128]
   (cf exp2_acc.sollya) with 105.765 bits of relative precision.
   Coefficients of degree 0-4 are double-double, 5-9 are doubles. */
static double
cr_exp2_accurate (double x, int e, int i)
{
  double h, l, yh, yl;

  /* we recompute h, l such that x = e + i/128 + h + l,
     to get more accuracy on l (in the fast path we extract the high 8 bits
     of h to go into i, thus we lose 8 bits on l) */
  double l1;
  double ah, al, bh, bl;
  ah = x; al = 0;
  bh = 0; bl = 0;
  ah -= e + i / 128.0;
  /* h + l = ah + (al + bh) + (bl + c) */
  fast_two_sum (&h, &l, ah, al);
  fast_two_sum (&h, &l1, h, bh);
  /* h + (l+l1) = ah + (al + bh) */
  l += l1 + bl;

  /* now x ~ e + i/128 + h + l */

  static const double p[14] = { 0x1p0,            /* p[0]: degree 0 */
    0x1.62e42fefa39efp-1, 0x1.abc9e3b397ebp-56,   /* p[1,2]: degree 1 */
    0x1.ebfbdff82c58fp-3, -0x1.5e43a5429b326p-57, /* p[3,4]: degree 2 */
    0x1.c6b08d704a0cp-5, -0x1.d331600cee073p-59,  /* p[5,6]: degree 3 */
    0x1.3b2ab6fba4e77p-7, 0x1.4fb30e5c2c8bcp-62,  /* p[7,8]: degree 4 */
    0x1.5d87fe78a6731p-10,                        /* p[9]: degree 5 */
    0x1.430912f86bfb8p-13,                        /* p[10]: degree 6 */
    0x1.ffcbfc58b51c9p-17,                        /* p[11]: degree 7 */
    0x1.62c034be4ffd9p-20,                        /* p[12]: degree 8 */
    0x1.b523023e3d552p-24 };                      /* p[13]: degree 9 */
  double t, u;
  yh = p[12] + h * p[13];
  yh = p[11] + h * yh;
  yh = p[10] + h * yh;
  /* ulp(p6*h^6) <= 2^-107, thus we can compute h*yh in double,
     but ulp(p5*h^5) <= 2^-97, so we need a double-double to store
     p5 + h*yh */
  fast_two_sum (&yh, &yl, p[9], h * yh);
  /* multiply (yh,yl) by h and add p4=(p[7],p[8]) */
  dekker (&t, &u, yh, h);
  u += yl * h;
  fast_two_sum (&yh, &yl, p[7], t);
  yl += u + p[8];
  /* multiply (yh,yl) by h and add p3=(p[5],p[6]) */
  dekker (&t, &u, yh, h);
  u += yl * h;
  fast_two_sum (&yh, &yl, p[5], t);
  yl += u + p[6];
  /* multiply (yh,yl) by h and add p2=(p[3],p[4]) */
  dekker (&t, &u, yh, h);
  u += yl * h;
  fast_two_sum (&yh, &yl, p[3], t);
  yl += u + p[4];
  /* multiply (yh,yl) by h and add p1=(p[1],p[2]) */
  dekker (&t, &u, yh, h);
  u += yl * h;
  fast_two_sum (&yh, &yl, p[1], t);
  yl += u + p[2];
  /* multiply (yh,yl) by h and add p0=1 */
  dekker (&t, &u, yh, h);
  u += yl * h;
  fast_two_sum (&yh, &yl, p[0], t);
  yl += u;
  /* yh+yl is an approximation of 2^h */

  /* multiply by 2^l */
  static const double l2 = 0x1.62e42fefa39efp-1;
  t = l2 * l * yh;
  fast_two_sum (&yh, &u, yh, t);
  u += yl;

  /* multiply by 2^(i/128) */
  t = yh * tab_i[127+i][1];
  dekker (&yh, &yl, yh, tab_i[127+i][0]);
  yl += t + u * tab_i[127+i][0];
  d64u64 v;
  v.x = yh + yl;
  unsigned int f = v.n >> 52;
  f += e;
  if (__builtin_expect((f - 1) > 0x7ff, 0)) /* overflow or subnormal */
  {
    if (e > 0) /* overflow */
      return xmax + xmax;
    /* Same method as in the fast path. */
    double magic = ldexp (1.0, -1022 - e);
    fast_two_sum (&t, &u, magic, yh);
    t += u + yl; /* here comes the rounding */
    t -= magic;
    return ldexp (t, e);
  }

  /* rounding test */
  double err = 0x1p-104;
  double left = yh + (yl - err), right = yh + (yl + err);
  if (left == right)
  {
    return ldexp (v.x, e);
  }

  d64u64 w;
  w.x = x;
  switch (w.n & 0x3f) {
    case 2:
      if (x == 0x1.1380388fd8942p-33)
        return 0x1.000000005f7b3p+0 + 0x1.fffffffffffffp-54;
      if (x == -0x1.3ec814d260d02p-10)
        return 0x1.ff9190b6000bdp-1 - 0x1.fffffffffffffp-55;
      break;
    case 3:
      if (x == -0x1.06922ce606443p-25)
        return 0x1.ffffff49ffe97p-1 - 0x1.fffffffffffffp-55;
      if (x == -0x1.72e40977492c3p-8)
        return 0x1.fdfed7e7210e2p-1 + 0x1.03649710c1f49p-108;
      if (x == -0x1.72e40977492c3p-8)
        return 0x1.fdfed7e7210e2p-1 + 0x1.03649710c1f49p-108;
      break;
    case 5:
      if (x == -0x1.234ada2403885p-6)
        return 0x1.f9baa4e60c96fp-1 + 0x1.75fc83263fa78p-106;
      if (x == -0x1.234ada2403885p-6)
        return 0x1.f9baa4e60c96fp-1 + 0x1.75fc83263fa78p-106;
      break;
    case 7:
      if (x == 0x1.9cebb555ce547p-37)
        return 0x1.0000000008f1bp+0 + 0x1.fffffffffffffp-54;
      break;
    case 8:
      if (x == 0x1.8859f5e252908p-4)
        return 0x1.11930594f1671p+0 - 0x1.fffffffffffffp-54;
      break;
    case 9:
      if (x == -0x1.96d26a97d3ec9p-16)
        return 0x1.fffdcc079fa23p-1 - 0x1.fffffffffffffp-55;
      break;
    case 10:
      if (x == -0x1.ec1c7584af30ap-38)
        return 0x1.fffffffff5573p-1 - 0x1.fffffffffffffp-55;
      if (x == 0x1.82c7120ec258ap-15)
        return 0x1.000218323a373p+0 + 0x1.fffffffffffffp-54;
      break;
    case 12:
      if (x == -0x1.b444c224a70ccp-1)
        return 0x1.1ba39ff28e3eap-1 - 0x1.cb1ddc7fbf64ep-108;
      if (x == -0x1.b444c224a70ccp-1)
        return 0x1.1ba39ff28e3eap-1 - 0x1.cb1ddc7fbf64ep-108;
      break;
    case 13:
      if (x == 0x1.755aa6fa428cdp-9)
        return 0x1.008185c263cb5p+0 - 0x1.fffffffffffffp-54;
      if (x == 0x1.12eecf76d63cdp+1)
        return 0x1.1ba39ff28e3eap+2 - 0x1.cb1ddc7fbf64ep-105;
      if (x == 0x1.92eecf76d63cdp+1)
        return 0x1.1ba39ff28e3eap+3 - 0x1.cb1ddc7fbf64ep-104;
      break;
    case 14:
      if (x == -0x1.a1c205dcb368ep-20)
        return 0x1.ffffdbcdd6957p-1 - 0x1.fffffffffffffp-55;
      break;
    case 15:
      if (x == 0x1.9f1a7d355cb4fp+0)
        return 0x1.89d948a94fe17p+1 - 0x1.a658852e8fdp-104;
      break;
    case 16:
      if (x == 0x1.2eecf76d63cdp-3)
        return 0x1.1ba39ff28e3eap+0 - 0x1.cb1ddc7fbf64ep-107;
      break;
    case 19:
      if (x == 0x1.5c356347f1b53p-8)
        return 0x1.00f1ce05142bdp+0 - 0x1.fffffffffffffp-54;
      break;
    case 21:
      if (x == -0x1.899e0474ba2d5p-9)
        return 0x1.feef72f67c668p-1 + 0x1.4481acf37d0ebp-111;
      if (x == -0x1.899e0474ba2d5p-9)
        return 0x1.feef72f67c668p-1 + 0x1.4481acf37d0ebp-111;
      if (x == 0x1.f16d04608afd5p-7)
        return 0x1.02b53825c16d5p+0 - 0x1.adcbe036fa2fep-107;
      if (x == -0x1.899e0474ba2d5p-9)
        return 0x1.feef72f67c668p-1 + 0x1.4481acf37d0ebp-111;
      break;
    case 23:
      if (x == 0x1.25afe8f725317p-11)
        return 0x1.0019737447e57p+0 - 0x1.fffffffffffffp-54;
      break;
    case 24:
      if (x == 0x1.48ef3961ac098p-47)
        return 0x1.000000000001dp+0 - 0x1.fffffffffffffp-54;
      break;
    case 26:
      if (x == 0x1.449b3bfeb3ddap-45)
        return 0x1.0000000000071p+0 - 0x1.fffffffffffffp-54;
      if (x == 0x1.25dd9eedac79ap+0)
        return 0x1.1ba39ff28e3eap+1 - 0x1.cb1ddc7fbf64ep-106;
      break;
    case 27:
      if (x == -0x1.8177265c6649bp-10)
        return 0x1.ff7a79d5dd209p-1 - 0x1.fffffffffffffp-55;
      break;
    case 28:
      if (x == 0x1.ed937b32a891cp-8)
        return 0x1.015703f362a59p+0 + 0x1.fffffffffffffp-54;
      break;
    case 30:
      if (x == 0x1.7ab9ba54d881ep-31)
        return 0x1.000000020d067p+0 - 0x1.fffffffffffffp-54;
      if (x == 0x1.3e34fa6ab969ep-1)
        return 0x1.89d948a94fe17p+0 - 0x1.a658852e8fdp-105;
      break;
    case 31:
      if (x == -0x1.cef4c143b5adfp-1)
        return 0x1.11930594f1671p-1 - 0x1.fffffffffffffp-55;
      break;
    case 32:
      if (x == 0x1.c2c3ad10cdf6p-33)
        return 0x1.000000009c391p+0 + 0x1.fffffffffffffp-54;
      break;
    case 33:
      if (x == 0x1.6c4175ea0c6e1p-3)
        return 0x1.21969738ee035p+0 - 0x1.fffffffffffffp-54;
      break;
    case 36:
      if (x == -0x1.75fb048f853a4p-10)
        return 0x1.ff7e73c8f1e79p-1 - 0x1.ffffffffffffep-55;
      if (x == 0x1.93998f2295764p-21)
        return 0x1.000008be08875p+0 - 0x1.fffffffffffffp-54;
      break;
    case 37:
      if (x == -0x1.526ce079b05a5p-5)
        return 0x1.f18bf8b031dbdp-1 - 0x1.fffffffffffffp-55;
      if (x == 0x1.f99afefa30d65p-8)
        return 0x1.015f65d104a36p+0 - 0x1.c66e8905e401p-111;
      break;
    case 38:
      if (x == -0x1.94e8b9b72d9a6p-27)
        return 0x1.ffffffb9d5a89p-1 - 0x1.fffffffffffffp-55;
      if (x == -0x1.da22611253866p+0)
        return 0x1.1ba39ff28e3eap-2 - 0x1.cb1ddc7fbf64ep-109;
      if (x == -0x1.da22611253866p+0)
        return 0x1.1ba39ff28e3eap-2 - 0x1.cb1ddc7fbf64ep-109;
      break;
    case 39:
      if (x == 0x1.9aa887abfb167p-22)
        return 0x1.0000047296371p+0 + 0x1.fffffffffffffp-54;
      break;
    case 43:
      if (x == 0x1.b014253a7dd6bp-27)
        return 0x1.000000256fcffp+0 - 0x1.fffffffffffffp-54;
      break;
    case 45:
      if (x == -0x1.0a4529bfcfbedp-18)
        return 0x1.ffffa3b7c984fp-1 - 0x1.fffffffffffffp-55;
      if (x == -0x1.bc7a709d1a2adp-36)
        return 0x1.ffffffffd97d3p-1 - 0x1.fffffffffffffp-55;
      break;
    case 49:
      if (x == -0x1.35dd739305031p-6)
        return 0x1.f954f4f26d1ddp-1 + 0x1.fffffffffffffp-55;
      break;
    case 50:
      if (x == -0x1.47b667916c4b2p-9)
        return 0x1.ff1d0b30a8261p-1 + 0x1.5293b4bf9f174p-107;
      if (x == -0x1.47b667916c4b2p-9)
        return 0x1.ff1d0b30a8261p-1 + 0x1.5293b4bf9f174p-107;
      if (x == -0x1.47b667916c4b2p-9)
        return 0x1.ff1d0b30a8261p-1 + 0x1.5293b4bf9f174p-107;
      break;
    case 51:
      if (x == -0x1.ed11308929c33p+1)
        return 0x1.1ba39ff28e3eap-4 - 0x1.cb1ddc7fbf64ep-111;
      if (x == -0x1.6d11308929c33p+1)
        return 0x1.1ba39ff28e3eap-3 - 0x1.cb1ddc7fbf64ep-110;
      if (x == -0x1.ed11308929c33p+1)
        return 0x1.1ba39ff28e3eap-4 - 0x1.cb1ddc7fbf64ep-111;
      if (x == -0x1.6d11308929c33p+1)
        return 0x1.1ba39ff28e3eap-3 - 0x1.cb1ddc7fbf64ep-110;
      break;
    case 53:
      if (x == -0x1.d6518ead568f5p-47)
        return 0x1.fffffffffffafp-1 - 0x1.fffffffffffffp-55;
      if (x == 0x1.8d040898b73f5p-6)
        return 0x1.04560ec7b6c8dp+0 + 0x1.fffffffffffffp-54;
      break;
    case 58:
      if (x == 0x1.926961243babap-3)
        return 0x1.255a2a884ee79p+0 + 0x1.112edd525e11cp-105;
      if (x == 0x1.926961243babap-3)
        return 0x1.255a2a884ee79p+0 + 0x1.112edd525e11cp-105;
      break;
    case 59:
      if (x == -0x1.ff8970e2da87bp-4)
        return 0x1.d58af95f85766p-1 - 0x1.1c30e57dc9b05p-107;
      break;
    case 62:
      if (x == 0x1.71547652b82fep-53)
        return 0x1.0000000000001p+0 - 0x1.fffffffffffffp-54;
      break;
    case 63:
      if (x == -0x1.fe89353e31cbfp-3)
        return 0x1.aec09a0cd9ea4p-1 - 0x1.a549adf007feap-109;
      break;
  };

  return ldexp (v.x, e);
}

double
cr_exp2 (double x)
{
  d64u64 v;
  v.x = x;
  int e = ((v.n >> 52) & 0x7ff) - 0x3ff;

  if (e >= 10) /* Overflow or potential underflow. For e=9 we cannot
                  have any overflow or underflow. Indeed the largest
                  number with e=9 is x=0x1.fffffffffffffp+9: exp2(x)
                  does not overflow, and exp2(-x) does not underflow. */
  {
    if (isnan (x))
      return x + x; /* always return qNaN, even for sNaN input */

    if (x >= 1024.) /* 2^x > 2^1024*(1-2^-54) */
      return xmax + xmax;
    else if (x < -1074.) /* 2^x < 2^-1074 */
    {
      static const double xmin = 0x1p-1074;
      if(x < -1075.) /* 2^x < 2^-1075 */
	return xmin/2;
      return xmin*(1.0 + 0.5*(x+1074));
    }
    /* otherwise go through the main path */
  }

  double h, t = __builtin_trunc (128.0 * x), u;
  h = x - t / 128.0; /* exact */
  /* check if x is an integer, avoiding inexact exception */
  if (h == 0 && (double) (int) x == x) return ldexp (1.0, x);
  e = t;
  int i = e % 128;
  e = (e - i) >> 7;
  /* 2^x ~ 2^e * 2^(i/128) * 2^h where |h| < 1/128 and -127 <= i <= 127 */

  /* p[i] are the coefficients of a degree-6 polynomial approximating 2^x
     over [-1/128,1/128], with double coefficients, except p[1] which is
     double-double (here we give only the upper term of p[1], the lower
     term is p1l). The relative accuracy is 70.862 bits. */
  static const double p[7] = {
  0x1p0, 0x1.62e42fefa39efp-1, 0x1.ebfbdff82c58fp-3,
    0x1.c6b08d70484c1p-5, 0x1.3b2ab6fb663a2p-7, 0x1.5d881a764d899p-10,
    0x1.430bba9c70dddp-13 };
  static const double p1l = 0x1.b2ca0bb577094p-56;
  double hh = h * h;
  /* |h| < 2^-7 thus hh < 2^-14, and the error on hh is bounded by 2^-67 */
  double yl = p[5] + h * p[6];
  /* |h * p[6]| < 2^-7 * 2^-12 = 2^-19 thus the rounding error on h * p[6] is
     bounded by 2^-72. When adding to p[5] we stay in the same binade than p[5]
     thus error(yl) < ulp(p[5]) + 2^-72 = 2^-62 + 2^-72 < 2^-61.99 */
  double yh = p[3] + h * p[4];
  /* |h * p[4]| < 2^-7 * 2^-6 = 2^-13 thus the rounding error on h * p[4] is
     bounded by 2^-66. When adding to p[3] we stay in the same binade than p[3]
     thus error(yh) < ulp(p[3]) + 2^-66 = 2^-57 + 2^-66 < 2^-56.99 */
  yh = yh + hh * yl;
  /* the error on yh is bounded by:
     2^-56.99 : error on the previous value of yh
     2^-67 * 2^-9 = 2^-76 : error on hh times maximal value of yl
     2^-14 * 2^-61.99 = 2^-75.99 : maximal value of hh times error on yl
     2^-76 : rounding error on hh * yl since hh * yl < 2^-23
     2^-57 : rounding error on yh+... since we stay in the same binade as p[3]
     Total: error(yh) < 2^-55.99 */
  yh = p[2] + yh * h;
  /* |yh * h| < 2^-4 * 2^-7 = 2^-11 thus the rounding error on yh * h is
     bounded by 2^-64. After adding yh * h we stay in the same binade than p[2]
     thus the sum of the rounding errors in the addition p[2] + ... and the
     product yh * h is bounded by ulp(p2) + 2^-64 = 2^-55 + 2^-64 < 2^-54.99.
     We need to add the error on yh multiplied by h, which is bounded by
     2^-55.99 * 2^-7 < 2^-62.99, which gives 2^-55+2^-64+2^-62.99 < 2^-54.99.
     This error is multiplied by h^2 < 2^-14 below, thus contributes to at most
     2^-68.99 in the final error. */
  /* add p[1] + p1l + yh * h */
  fast_two_sum (&yh, &yl, p[1], yh * h);
  /* At input |yh * h| < 2^-2*2^-7 = 2^-9 thus the rounding error on yh * h is
     bounded by 2^-62. This rounding error is multiplied by h < 2^-7 below
     thus contributes to < 2^-69 to the final error.
     The fast_two_sum error is bounded by 2^-105 |yh| (for the result yh),
     thus since |yh| <= o(p[1] + 2^-9) <= 2^-0.52, the fast_two_sum error is
     bounded by 2^-105.52. Since this error is multiplied by h < 2^-7 below,
     it contributes to < 2^-112.52 to the final error. */
  yl += p1l;
  /* |yl| < 2^-53 and |p1l| < 2^-55 thus the rounding error in yl += p1l
     is bounded by 2^-105 (we might have an exponent jump). This error is
     multiplied by h below, thus contributes to < 2^-112. */
  /* multiply (yh,yl) by h */
  dekker (&yh, &t, yh, h); /* exact */
  /* add yl*h */
  t += yl * h;
  /* |yl| < 2^-52 here, thus |yl * h| < 2^-59, thus the rounding error on
     yl * h is < 2^-112.
     |yh| < 1 before the dekker() call, thus after we have |yh| < 2^-7
     and |t| < 2^-60, thus |t + yl * h| < 2^-58, and the rounding error on
     t += yl * h is < 2^-111. */
  /* add p[0] = 1 */
  fast_two_sum (&yh, &yl, p[0], yh);
  u = yl + t;
  /* now |yh| < 2 and |yl| < 2^-52, with |t| < 2^-58, thus |yl+t| < 2^-51
     and the rounding error in u = yl + t is bounded by 2^-104.
     The error in fast_two_sum is bounded by 2^-105 |yh| <= 2^-104. */
  /* now (yh,yl) approximates 2^h to about 68 bits of accuracy:
     2^-68.99 from the rounding errors for evaluating p[2] + ...
     2^-69 from the rounding error in yh * h in the 1st fast_two_sum
     2^-112.52 from the error in the 1st fast_two_sum
     2^-112 from the rounding error in yl += p1l
     2^-112 from the rounding error in yl * h
     2^-111 from the rounding error in t += yl * h
     2^-104 from the rounding error in u = yl + t
     2^-104 from the error in the 2nd fast_two_sum
     Total absolute error < 2^-67.99 on yh+u here (with respect to 2^h).
  */

  /* multiply (yh,u) by 2^(i/128) */
  /* the maximal error |2^(i/128) - tab_i[127+i][0] - tab_i[127+i][1]|
     is 1/2*max(ulp(tab_i[127+i][1])) = 2^-107.
     Since we multiply by |2^h| < 1.006 this yields 2^-106.99. */
  t = yh * tab_i[127+i][1];
  /* |yh| < 1.006 and |tab_i[127+i][1]| < 0x1.fc6f89bd4f6bap-54 (i=78) thus
     |yh * tab_i[127+i][1]| < 2^-53 and the rounding error on t is
     bounded by 2^-106 */
  dekker (&yh, &yl, yh, tab_i[127+i][0]); /* exact */
  /* now |yh| < 2 thus |yl| < 2^-52, |t| < 2^-53 */
  yl += t + u * tab_i[127+i][0];
  /* |u| < 2^-51 and |tab_i[127+i][0]| < 2 thus |u * tab_i[127+i][0]| < 2^-50
     and the rounding error on u * tab_i[127+i][0] is bounded by 2^-103.
     |t + u * tab_i[127+i][0]| < 2^-49 thus the rounding error when adding t
     is bounded by 2^-102.
     |yl + t + u * tab_i[127+i][0]| < 2^-48 thus the rounding error in
     yl += ,,, is bounded by 2^-101.
     Total error bounded by:
     2^-66.997 (initial error 2^-67.99 on yh+u multiplied by maximal value of
                2^(i/128) for i=127)
     2^-105.99 for the error on 2^(i/128) multiplied by |yh+u| < 2
     2^-106 for the rounding error on t
     2^-103 for the rounding error on u * tab_i[127+i][0]
     2^-102 for the rounding error on t + u * tab_i[127+i][0]
     2^-101 for the rounding error on yl += ...
     Total error < 2^-66.99 on yh + yl with respect to 2^(i/128+h+l). */

  /* now yh+yl approximates 2^(i/128+h+l) with error < 2^-66.99 */

  /* rounding test */
  double err = 0x1.01c7d6c404f0cp-67; /* e = up(2^-66.99) */
  v.x = yh + (yl - err);
  double right = yh + (yl + err);
  if (v.x != right)
    return cr_exp2_accurate (x, e, i);

  /* Multiply by 2^e. */
  unsigned int f = v.n >> 52; /* sign is always positive */
  f += e;
  if (__builtin_expect((f - 1) > 0x7ff, 0)) /* overflow or subnormal */
  {
    if (e > 0)
      return ldexp (v.x, e);
    /* In the subnormal case, we use the method mentioned by Godunov in his
       IEEE Transactions on Computers article (2020): add 2^-1022, and then
       subtract 2^-1022. Since we should multiply yh+yl by 2^e, to avoid
       underflow before adding/subtracting 2^-1022, we do it directly on
       yh+yl, where 2^-1022 is replaced by 2^(-1022-e). */
    double magic = ldexp (1.0, -1022 - e), left;
    /* In the following fast_two_sum call, magic and yh do overlap, and in
       this case it can be proven that fast_two_sum is exact, even for
       directed roundings. */
    fast_two_sum (&left, &u, magic, yh);
    right = left;
    left += u + (yl - err); /* here comes the rounding */
    left -= magic;
    right += u + (yl + err); /* here comes the rounding */
    right -= magic;
    if (left == right)
      return ldexp (left, e);
    return cr_exp2_accurate (x, e, i);
  }
  v.n += (int64_t) e << 52;
  return v.x;
}
