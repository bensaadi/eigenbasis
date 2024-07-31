#### Building

```
mkdir build && cd build
cmake ..
make
```

This will build the header-only libraries and test suites.


#### Purpose
This is a long-term project devoted to building open-source trading technologies that meet state-of-the-art performance and reliability standards. The idea is to provide building blocks that fintech companies can reuse to build the next class of innovative financial products.

#### Approach
Building small, reusable, and testable components and abstractions that remain relevant as use cases and technologies evolve.

#### Modules

| module             | lang | description                                                                                                                             | release  |
| ------------------ | ---- | --------------------------------------------------------------------------------------------------------------------------------------- | -------- |
| **book**           | C++  | a modular, extensible, high-throughput limit order book                                                                                 | released |
| **depth**          | C++  | an aggregate depth order book with arbitrary precision and number of levels                                                             | released |
| **margin-utils**   | C++  | a set of utility classes for margin trading and automatic liquidation                                                                   | upcoming |
| **mm-quotes**      | C++  | generates orders given a stream of quotes from market makers                                                                            | upcoming |
| **router**         | C++  | seamless, real-time routing of orders to multiple external exchanges. integrates with the limit order book via the *routable* plugin.   | upcoming |
| **observer**       | C++  | a template-based wrapper for implementing the observer pattern. Can use Intel TBB Concurrent Queues or lock-free queues under the hood. | upcoming |
| **ohlc**           | C++  | incremental generation of OHLC data and indicators given a stream of trade data.                                                        | upcoming |
| **clearing-house** | C++  | real-time balance settlement and netting given a stream of trade data. also performs fees/rebates calculations                          | upcoming |
| **wsfix**          | C++  | streams market data via WebSocket compressed with the FAST algorithm. includes a WebAssembly package for decompression                  | upcoming |
| **depth-chart**    | JS   | a real-time, interactive depth chart built with D3                                                                                      | upcoming |

### Contributors

This project is created and currently maintained by [L. Bensaadi](https://bensaadi.com/). If you are interested in contributing, feel free to [contact me](https://bensaadi.com/contact/).