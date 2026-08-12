# Hrvoje Ljubas: GC-DEV-04 · Project Slipstream: Volume-Aware Execution Engine

- Project Brief
    
    **Track:** Software Development / Quant Developer.
    
    **Difficulty tier:** Intermediate–Advanced. Assumes strong C++ and comfort with sockets; the hard parts are correctness under adversarial input and defending data-structure choices.
    
    <aside>
    🚧
    
    If you’re not comfortable with sockets, you should struggle to become comfortable with them. There is a section detailing hints for you below: C++ Networking Constructs. After this, you can expand the scope of the project by pursuing the use of gRPC, which abstracts that away from you. You **may** be asked about sockets, file descriptors, and more networking-related concepts in a Quant Development interview.
    
    </aside>
    
    **Target roles:** Quant developer / low-latency C++ engineer at Jane Street, Citadel Securities, HRT, Jump, Optiver, IMC.
    
    **Skills tested:** 
    
    - binary wire-format parsing and frame reassembly over a byte stream
    - fixed-point arithmetic and accumulator overflow bounds
    - O(1) amortized rolling-window state
    - order state machines with in-flight accounting (optional), timeouts and reconciliation (optional)
    - latency measurement and tail analysis;
    - interface extraction / dependency inversion for a transport swap
    - written reasoning quality in a README.
    - ability to handle ambiguity.

<aside>
🎯

Build a single-instrument execution algorithm in C++ that validates trades made against a rolling VWAP window based on quotes submitted by market participants.

**Estimated effort** is detailed below, broken down by candidate skill-level.

</aside>

| Track | Level | Language | Est. |
| --- | --- | --- | --- |
| Software Development | Intermediate–Advanced | Server in C++, clients in either C++ or Python | Depending on skill level, anywhere between 30 to 85 hours+. |

<aside>
⚙

The core server should be written in C++. The client responsible for sending quotes, and the client responsible for sending trades and receiving execution reports, can ****be written in Python.

</aside>

---

---

# Summary

Build a single-instrument execution algorithm (server application) in modern C++ that consumes a binary market data feed over a stream socket, maintains a rolling VWAP benchmark and an L1 book, and works an order into the market under a participation constraint.

**Estimated effort:** 

| Skill Level | Estimated Time Required |
| --- | --- |
| Beginner | 65-80+ hours |
| Intermediate | 40-65 hours |
| Advanced | 25-40 hours |

**Language:** C++20 minimum (C++23 encouraged). **Platform:** Linux. **Recommended IDE:** CLion

**Why this project:** there are themes that arrive in almost every quant dev interview process:

1. can you parse a binary wire format correctly (networking)
2. can you pick the right data structure under a latency constraint (DSA + performance)
3. can you reason about state machines that touch real money or important events (concurrency + design), and 
4. can you talk about your own tradeoffs. (logical reasoning + testing)

This project forces all four. It is also a good portfolio artifact: it is small enough to explain in ten minutes and deep enough that an interviewer can drill for sixty.

---

# 1. The Scenario

You have joined the execution team at a systematic trading firm. The alpha team hands you a list of **historical** trades and quotes, which you will use to perform an analysis over.

The trades are yours (your firms), and the quotes belong to market participants. Your benchmark is **VWAP** (volume weighted average price). If your average fill price beats the market's VWAP over the same period, you added value, you should have traded. If it's worse, you cost the firm money, and you should have never traded. You are also capped on:

1. **participation**: you may never account for more than a set fraction of the volume that has traded, because a strategy that becomes the whole market is a strategy that gets identified as coming from a single source. This allows competitors to more easily attribute trading activity to your organization.
2. **order size:** Your trades cannot be larger in quantity than `--max-quantity` .

You are building the process that does this, from the wire up.

---

# 2. What You Build

A single executable, `slipstream`, and the two clients (total three programs / executables), that:

1. Connects to a market data and order entry client over TCP and consumes a binary feed of quotes, trades, heartbeats, and session-control messages.
    1. The quotes and trades will be generated from this example Excel sheet with an embedded CSV. 
    
    Quotes and Trades.csv
    
    <aside>
    ❗
    
    The CSV contains some additional detail above the header row to make better sense of its contents. Remove said details, or create a copy, for client ingestion (see below).
    
    </aside>
    
    1. Heartbeats, and session control message specifications are detailed below.
2. Two clients, either in C++ or Python. The following are each of their responsibilities:
    1. **Order Entry (OE) client**: Connects to an order gateway over TCP - **bidirectional**: you send orders, it sends back NewOrder messages. The order gateway is the main application server (`slipstream`).
        1. Orders (trades) come from the CSV linked above, which contains both trades and quotes.
    2. **Market Data (MD) client:** Connects to the order gateway over TCP - **unidirectional**: you send quotes to the order gateway (`slipstream`).
        1. Market data (quotes) come from the CSV linked above.
        2. TCP is not needed, given unidirectional communication. A stretch goal detailed below is to use UDP.
3. Both clients need to send trades and quotes based on relative wall-time. An example can best illustrate this:
    1. If the first row is a quote that goes out at 9:30.000AM, and the next is a trade goes out on 9:30.500AM, your MD client will send the quote to `slipstream` immediately, but your OE client will sent it 500 milliseconds after it starts up.
4. Maintains a **rolling** VWAP over a trailing time window of trade prints inside the order gateway.
5. Maintains the L1 (top of book) quote state per symbol.
6. Decides, on every quote update, whether the trade price is favorable on two metrics:
    1. Is the trade price favorable by more than X basis points (`--bands-bps`)? For instance:
        1. If the VWAP is $10, basis point is 50, and you bought at $9.99, this isn’t enough of an edge ($0.01 is less than a 0.5% of $10). 
        2. If the VWAP is $10, basis point is 50, and you bought at $11, you actually bought at a **more expensive price** that the prevailing market VWAP. In this case, you *definitely* should have **not** traded.
    2. Would the trade have caused you to break through the `--participation-cap`? For instance,
        1. If the `vwap-window-ms` is 30’000, and you are already responsible for 14.9% of volume over this period, with a participation cap of `0.15` (15%) we should reject this trade (it should not have happened).
7. When it identifies a trade that your alpha team **correctly** took, it sends a `NewOrder` message to your OE client with `status` of `‘A’` (ACCEPTED), otherwise send the same message with `status` of `'R'` (REJECTED).
8. [Optional] Tracks order state through acknowledgement, fill, partial fill, rejection, and timeout; responding exclusively to the OE client. 
9. Terminates on either client completion or session close, and prints an execution report, either to file or to console.

<aside>
⚙

The `slipstream` needs both clients to run. You should start your `slipstream` first, then your clients. When either client disconnects (either due to session close or the CSV being exhausted), the `slipstream` closes the other connection and prints its completion details.

</aside>

https://excalidraw.com/#json=TpRtQ7xrxDrOTT808MAh2,oTn_-shGZHqrpMW5nJdqUA

## 2.1 Command line interface

```bash
slipstream \
  --symbol    SYNTH1 \
  --max-quantity  500 \
  --participation-cap 0.15 \
  --vwap-window-ms 30000 \
  --band-bps       25.5 \
  --md-host 127.0.0.1 --md-port 14200 \
  --oe-host 127.0.0.1 --oe-port 14300 \
  --transport      tcp
```

| Argument | Meaning |
| --- | --- |
| `--symbol` | Instrument to trade. Ignore messages for any other symbol. |
| `--max-quantity`   | Ceiling on any trade. |
| `--participation-cap` | Fraction in `[0,1]`. Cumulative executed qty must stay ≤ cap × cumulative market volume observed since start. |
| `--vwap-window-ms` | Length of the **trailing rolling window** used for the VWAP benchmark. |
| `--band-bps` | Required edge, in basis points, before you will cross. One basis point is 0.01% |
| `--transport` | `tcp` (required), `grpc` / `udp-multicast` (Milestone 4). |
| `--md-[host|port]` | The host + port combination of the client responsible for reading the CSV and sending **quotes.** |
| `--oe-[host|port]` | The host + port combination of the client responsible for reading the CSV and sending **trades.** |

> ⚠️ Note the rolling window. This is not a tumbling window that resets every N seconds - at every tick, trades older than `now − vwap-window-ms` fall out of the calculation. Getting this to O(1) amortized is an exercise that comes up in many Leetcode-like coding questions asked at firms. O(1) is the goal and is part of the exercise.
> 

## 2.2 Decision rules

**Warm-up gate.** From `slipstream`, send nothing until *both*: (a) the feed has been running for at least one full `vwap-window-ms`, and (b) at least 10 quotes have been observed. A VWAP computed from less than 10 quotes is noise, and an algo that acts on it is a liability.

**Trigger condition.** Let `V` be the current rolling VWAP and `B = band_bps / 10000`.

- **BUY:** trigger when `best_ask_px ≤ V × (1 − B)`
- **SELL:** trigger when `best_bid_px ≥ V × (1 + B)`

A historic trade is valid if it has met this trigger condition, along with the afformentioned conditions relating to quantity and participation.

**Session control [optional].** Your client should support more than just reading and sending wall-time-relative respecting messages. It should also accept input a `SessionControl` message from the server.

The server should allow for user input. If the user types:

- `HALT`: stop sending new orders, the server will keep consuming and updating VWAP in the meantime.
- `OPEN` after halt: resume.
- `CLOSE`: flush, close the connection, exit cleanly.

**Heartbeats.** If `slipstream` receives no message of any kind from the OE client for at least 5 seconds, log a staleness warning and send the client a `heartbeat` message, specified below. The OE client should print its details to console.

## 2.3 Execution report (stdout, on exit)

On exit, prompted by a client closing their session, the `slipstream` should gracefully disconnect before printing an execution report.

```
=== SLIPSTREAM EXECUTION REPORT ===
symbol              SYNTH1
market qty          50000
executed qty        47300   (94.60%)
avg fill price      101.2438
session VWAP        101.2701
slippage vs VWAP    -2.60 bps   (favorable)
participation       11.83%  (cap 15.00%)
tick-to-order p50   1.9 us
tick-to-order p99   6.4 us
tick-to-order p99.9 21.7 us
```

---

<aside>
❗

`tick-to-order` is a timestamp value computed in `slipstream` . It’s an internal metric that tracks the difference between each order send time and trade receive time. The percentile values tell us just how latent our decision engine (`slipstream`) was.

</aside>

# 3. Wire Protocol - `GCMD/1`

The following section details the communication protocol between your clients and the `slipstream` server.

<aside>
⚙

Alternative approaches to pure-handrolled binary are stated below in section 7. Networking Layer - Required and Alternative Approaches. It’s important that you start with the bare-bones socket-level programming.

</aside>

- C++ Networking Constructs
    
    The following is a hint for the sort of constructs you should familiarize yourself as you research through this.
    
    ## TCP Concept Cheatsheet
    
    1. TCP is a reliable *byte* stream, not a message stream.
    
    The kernel guarantees your one-time in-order delivery. It guarantees nothing about *how they are grouped*. A `send()` of one 44-byte quote can arrive as a 20-byte read and a 24-byte read. Three quotes sent separately can arrive as one 132-byte read. Both are correct TCP behavior.
    
    1. TCP byte stream require framing and reassembly.
    
    If you haven’t done this **problem**, you should. It tests the crux of TCP stream reassembly. 
    
    **The `recv()` return-value can signifity three outcomes.**
    Three distinct outcomes, and people routinely collapse them into two:
    
    - `> 0` - that many bytes arrived. **Not necessarily a whole message.**
    - `== 0` - the peer performed an orderly shutdown. This is a *connection closed* event, not an error and not "no data available."
    - `< 0` - check `errno`. `EAGAIN`/`EWOULDBLOCK` on a non-blocking socket means "nothing right now," which is normal and not an error. `EINTR` means a signal interrupted you; retry.
    
    ## C++ TCP API Cheatsheet
    
    Keep these APIs in mind as you plan out your networking layer.
    
    | Concern | What to look for |
    | --- | --- |
    | Connect | `socket()`, `connect()`, `struct sockaddr_in`, `inet_pton()`, `htons()` for the port |
    | Non-blocking | `fcntl()` with `O_NONBLOCK` |
    | Non-blocking connect | `connect()` returns `-1` with `errno == EINPROGRESS`; wait for writability via `select`/`poll`, then `getsockopt(SO_ERROR)` to get the real result |
    | Latency | `setsockopt(TCP_NODELAY)` |
    | Buffers | `SO_RCVBUF`, `SO_SNDBUF` |
    | Liveness | `SO_KEEPALIVE`, plus your own application heartbeat |
    | `slipstream` server | `bind()`, `listen()`, `accept()`, `SO_REUSEADDR` |
    | Send flags | `MSG_NOSIGNAL`, `MSG_DONTWAIT` |
    | Teardown | `shutdown()` vs `close()` , know the difference |
    
    There are a couple of C++ concepts you will also need to investigate that are networking-adjacent. The following are important considerations.
    
    | Concern | What to look for |  |
    | --- | --- | --- |
    | Endianness | The protocol is little-endian. Check your understanding of that **here**. |  |
    | Struct packing | The wire has no padding; your compiler adds some by default. Look into `#pragma pack` to ensure that messages aren’t wasting space, and are formatted according to the specifications above. |  |
    | Strict aliasing | `reinterpret_cast`ing a `char*` receive buffer to a message struct pointer is undefined behavior. Use `std::memcpy` for individual field, or `std::bit_cast` for entire-struct copying / interpreting. |  |
    | Unaligned access | Fields at odd offsets in a packed struct are unaligned. Accessing them through a pointer are fine on x86 but can fault on ARM. |  |
    | Fixed-width types | Notice how all the types specified in the messages above are fixed-width. For instance, `uint32_t`, not `unsigned int`. `int` is not a wire type. |  |
    | RAII | As you’re handling file descriptors, just wrap them with a small type that contains a destructor to close them on destruction, and a deleted copy constuctor to prevent them from being copied. |  |

All integers **little-endian**. Structures are **packed** - no compiler padding. Prices are **fixed-point signed 64-bit integers scaled by 10,000** (so `101.2500` is transmitted as `1012500`). Do not use floating point in your calculations; conversion back to floating point should only be used when displaying data to the user (a human). 🙂

## 3.1 Frame header - 4 bytes, precedes every market data message

| Field | Type | Bytes | Notes |
| --- | --- | --- | --- |
| `body_len` | `uint16` | 2 | Length of body only; header excluded |
| `msg_type` | `uint8` | 1 | `1`=Quote, `2`=Trade, `3`=Heartbeat, `4`=SessionControl |
| `version` | `uint8` | 1 | Always `1` for this protocol revision |

## 3.2 Quote body - 44 bytes

| Field | Type | Bytes | Notes |
| --- | --- | --- | --- |
| `symbol` | `char[12]` | 12 | ASCII, null-padded, not null-terminated if full |
| `ts_ns` | `uint64` | 8 | Nanoseconds since Unix epoch |
| `bid_qty` | `uint32` | 4 |  |
| `bid_px` | `int64` | 8 | Fixed point ×10,000 |
| `ask_qty` | `uint32` | 4 |  |
| `ask_px` | `int64` | 8 | Fixed point ×10,000 |

## 3.3 Trade body - 41 bytes

| Field | Type | Bytes | Notes |
| --- | --- | --- | --- |
| `symbol` | `char[12]` | 12 |  |
| `ts_ns` | `uint64` | 8 | Nanoseconds since Unix epoch |
| `qty` | `uint32` | 4 |  |
| `px` | `int64` | 8 | Fixed point ×10,000 |
| `aggressor` | `char` | 1 | `'B'`, `'S'`, or `'?'` if unknown |
| `id` | `int64` | 8 | Trade identifier |

## 3.4 Heartbeat body - 8 bytes

| Field | Type | Bytes | Notes |
| --- | --- | --- | --- |
| `ts_ns` | `uint64` | 8 | Server send time |

## 3.5 SessionControl body - 9 bytes

| Field | Type | Bytes | Notes |
| --- | --- | --- | --- |
| `ts_ns` | `uint64` | 8 |  |
| `state` | `uint8` | 1 | `0`=OPEN, `1`=HALT, `2`=CLOSE |

---

# 4. Order Protocol - `GCOE/1`

Bidirectional over a single TCP connection. Same 4-byte frame header as above, with its own message types.

## 4.1 NewOrder (slipstream → OE client) - `msg_type = 10`, body 42 bytes

| Field | Type | Bytes | Notes |
| --- | --- | --- | --- |
| `client_order_id` | `uint64` | 8 | Strictly increasing, unique per process run |
| `symbol` | `char[12]` | 12 |  |
| `status` | `char` | 1 | `A` for ACCEPTED, `R` for REJECTED |
| `ts_ns` | `uint64` | 8 | Your send time, either relative to the trade (harder) or absolute (easier). |
| `trade_id` | `int64` | 8 | The ID associated with the trade that generated this `NewOrder`. |
| `side` | `char` | 1 | `'B'` or `'S'` |
| `qty` | `uint32` | 4 |  |
| `limit_px` | `int64` | 8 | Fixed point ×10,000 |

## 4.2 ExecReport (slipstream → OE client) - `msg_type = 11`, body 30 bytes [optional]

| Field | Type | Bytes | Notes |
| --- | --- | --- | --- |
| `client_order_id` | `uint64` | 8 | Echo of the order this refers to |
| `ts_ns` | `uint64` | 8 | Gateway timestamp |
| `status` | `uint8` | 1 | `0`=ACK, `1`=FILL, `2`=PARTIAL, `3`=REJECT |
| `filled_qty` | `uint32` | 4 | Cumulative for this order |
| `avg_px` | `int64` | 8 | Fixed point ×10,000 |
| `reason_code` | `uint8` | 1 | `0`=none, `1`=risk, `2`=price, `3`=size, `4`=throttle |

**[Optional] Timeout rule.** If no ExecReport arrives for an order within 250 ms, mark it `TIMED_OUT`, release its `in_flight_qty`, and log it. If a report arrives afterward, reconcile - do not double-count. This is the single most realistic part of the exercise and the part interviewers ask about most.

---

# 5. Test Data Format

Your OE client and MD client reads a plain-CSV file and emits the binary protocol. Prices in the file are decimal; convert to fixed point on the way out.

```
# Format: Timestamp,Type,Symbol,BidPrice,BidQty,AskPrice,AskQty (for quotes)
# Format: Timestamp,Type,Symbol,Price,Qty (for trades)
# Timestamp will be in HH:MM:SS.mmm format
#
# Symbols in this file (each has a distinct microstructure regime):
#   SYNTH1  ~101.25  liquid, mean-reverting, tight spread
#   SYNTH2  ~248.50  steady upward drift, moderate liquidity
#   SYNTH3   ~87.40  thin book, wide spreads, sparse prints
#   SYNTH4   ~34.20  volatile, heavily traded, one-tick spread
#   SYNTH5 ~1250.00  high-priced, small sizes, wide absolute spread
#
# Rows are sorted by timestamp across all symbols, as a consolidated feed
# would deliver them. Same-millisecond events occur and are intentional.

Timestamp,Type,Symbol,BidPrice,BidQty,AskPrice,AskQty,Price,Qty
09:30:00.003,Q,SYNTH3,87.37,55,87.49,50,,
09:30:00.160,Q,SYNTH2,248.48,200,248.53,150,,
09:30:00.173,Q,SYNTH4,34.18,900,34.19,1100,,
09:30:00.190,T,SYNTH2,,,,,248.53,65
09:30:00.215,Q,SYNTH1,101.23,175,101.25,150,,
09:30:00.241,Q,SYNTH5,1249.62,6,1249.95,3,,
09:30:00.586,Q,SYNTH1,101.24,275,101.25,225,,
09:30:00.675,Q,SYNTH4,34.19,1000,34.20,1100,,
09:30:01.540,Q,SYNTH4,34.21,200,34.22,600,,
09:30:01.840,Q,SYNTH4,34.22,500,34.23,200,,
09:30:02.015,Q,SYNTH1,101.23,125,101.25,250,,
```

You can choose to send every quote and trade, but remember, your server is configured to only care about a **single** symbol.

<aside>
❗

The biggest mistake most people will make here is that they’ll read the entire file in 1 go in under a second, and hammer the `slipstream` server with everything. Remember, this is a **historic replay** of data that needs to be send in wall-time-relative terms. Look at the last 2 `SYNTH4` records, there should be 300 real-world milliseconds between each MD client send.

</aside>

---

# 6. Milestones

Milestones

## M1 - Codec (Coder/Decoder) and CSV replay clients (OE/MD clients) · ~8-20h

- Build the `GCMD/1` and `GCOE/1` serialization layer, plus the two clients that feed it. The clients read the CSV, strip the descriptive rows above the header, convert decimal prices to fixed point ×10,000, and replay rows on a **wall-time-relative** schedule - 300 ms between the last two `SYNTH4` prints means 300 real-world milliseconds.
- **Explicit unit tests for frames split across `recv()` boundaries** - pass bytes treams of various sizes to your business logic and ensure your parser produces a single coherent message after one becomes present. I recommend you return `bytes_written` from your encoder.
- **Deliverable**: `libslipstream_codec` + MD client (quotes, unidirectional) + OE client (trades, bidirectional) + passing tests. There’s nothing related to sockets, threads, VWAP or order logic at this point.

## M2 - Server, L1 book, rolling VWAP · ~10-20h

- Stand up `slipstream` as the listening side: `bind()`/`listen()`/`accept()` on both the MD and OE ports, frame reassembly per stream, L1 top-of-book state, and the rolling VWAP over the trailing `--vwap-window-ms`.
- Filter on `--symbol` and show you discard the other four symbols cleanly. Choose your window structure deliberately and defend it: a ring buffer of prints with head eviction gives O(1) amortized; a `std::deque` is simpler but allocates; a fixed-capacity circular buffer with a documented overflow policy is what most production systems actually do.
    - Consider embedding your policy as a non-type template parameter.
- Accumulate `Σ(px × qty)` in a 128-bit integer or prove your bounds - a 64-bit accumulator overflows fast.
- **Deliverable**: server that consumes both clients and reports a correct rolling VWAP across the replay.

## M3 - Decision engine and order path · ~10-25h

- The judgement layer: warm-up gate (one full window **and** at least 10 quotes), the band trigger (`best_ask_px ≤ V × (1 − B)` for BUY, `best_bid_px ≥ V × (1 + B)` for SELL), the `--max-quantity` ceiling, and the cumulative `--participation-cap` check.
- On a trade your alpha team correctly took, emit `NewOrder` to the OE client.
    - Ensure that you always send a `NewOrder` with a status field of either `ACCEPTED` or `REJECTED`  (`A` or `R`).
- Add `SessionControl` handling (HALT stops new orders while VWAP keeps updating, OPEN resumes, CLOSE flushes and exits). This is sent from the `slipstream` server to the OE client and should be triggered via `slipstream`'s console.
- The 5-second staleness heartbeat back to the OE client, and graceful teardown of the surviving connection.
- **Deliverable**: end-to-end runnable `slipstream` plus the printed execution report.

## M4 - Performance and transport swap · ~8-15h+

- Instrument `tick-to-order` - trade receive to order send, measured inside `slipstream` - and report p50/p99/p99.9.
- **[Optional]** Extract an abstract transport interface and implement a second one (see Section 7); gRPC or UDP multicast on the unidirectional market data leg are the natural candidates.
- **Deliverable**: benchmark harness, latency numbers, and the same decision engine running unmodified over two transports.

---

# 7. Networking Layer - Required and Alternative Approaches

Handrolling the TCP component using sockets initially is **required**. The core handler for your TCP code should be (near) identical to the one below.

Extract an interface first:

```cpp
class IFeedTransport {
public:
    virtual ~IFeedTransport() = default;
    virtual void start(std::function<void(const MdMessage&)> on_message) = 0;
    virtual void stop() = 0;
};

// Alternatively, on a higher level.
class IFeedTransport {
public:
		virtual ~IFeedTransport() = default;
		virtual void OnMessage(const std:variant<MdMessage, OeMessage>& msg) = 0;
};
```

The strategy must not change when the transport does. That is dependency inversion, and it is the concrete thing to point at when discussing decoupling and design.

- Option A - Raw TCP with blocking sockets *(required)*
    
    The baseline. One thread, `recv()` into a buffer, reassemble frames, dispatch. Cheap to write, easy to reason about, correct. Every candidate should be able to do this from memory.
    
    **Cost:** a syscall per read, a context switch per wakeup, no batching.
    
- Option B - `epoll` event loop
    
    Multiplex the market data socket and the order socket in one non-blocking loop. Now you can handle ExecReports and quotes without a second thread or a blocking-read deadlock.
    
    **Cost:** more code, and you must handle `EAGAIN` and partial writes properly.
    
    <aside>
    💬
    
    **Interview payoff:** "Why did you move off blocking sockets?" - the honest answer is that with two sockets and a 250 ms order timeout you need a timer and a multiplexer, and threads-plus-mutexes buys you a lock in the hot path you don't want.
    
    </aside>
    
- Option C - gRPC *(strongly recommended as the M4 comparison)*
    
    Define the feed as a server-streaming RPC and the order path as a bidirectional stream:
    
    ```protobuf
    // Pseudocode
    syntax = "proto3";
    package slipstream;
    
    message Quote  { string symbol = 1; uint64 ts_ns = 2;
                     uint32 bid_qty = 3; int64 bid_px = 4;
                     uint32 ask_qty = 5; int64 ask_px = 6; }
    message Trade  { string symbol = 1; uint64 ts_ns = 2;
                     uint32 qty = 3; int64 px = 4; string aggressor = 5; }
    message MdEvent { oneof event { Quote quote = 1; Trade trade = 2;
                                    uint64 heartbeat_ns = 3; uint32 session_state = 4; } }
    
    message NewOrder  { uint64 client_order_id = 1; string symbol = 2; uint64 ts_ns = 3;
                        string side = 4; uint32 qty = 5; int64 limit_px = 6; }
    message ExecReport { uint64 client_order_id = 1; uint64 ts_ns = 2; uint32 status = 3;
                         uint32 filled_qty = 4; int64 avg_px = 5; uint32 reason_code = 6; }
    
    service MarketData { rpc Subscribe (SubscribeRequest) returns (stream MdEvent); }
    service OrderEntry { rpc Trade (stream NewOrder) returns (stream ExecReport); }
    ```
    
    **What you get:** schema evolution without breaking old clients, generated bindings in any language, TLS and auth for free, deadlines and cancellation, flow control, and no hand-rolled framing bugs - which are the single largest source of defects in Option A.
    
    **What it costs:** HTTP/2 framing; protobuf serialization that allocates; head-of-line blocking on a single stream; and allocation jitter that shows up squarely in your p99.9. Expect tail latency measured in tens to hundreds of microseconds against single-digit microseconds for packed structs over a raw socket.
    
    **Where real firms use this:** gRPC in the control plane - risk queries, config distribution, position services, historical data, internal service-to-service RPC. Almost never in the market data or order-entry hot path, where you find UDP multicast for feeds and native binary or FIX-over-TCP for orders.
    
    <aside>
    💬
    
    **Interview payoff:** "Would you use gRPC for market data?" is a filter question. The wrong answer is a flat no with no reasoning; the wrong answer is also an enthusiastic yes. The right answer is that you built both, measured them, and can name the number at which the tradeoff flips. Bring your p99.9 chart.
    
    </aside>
    
- Option D - UDP multicast with sequence gap detection for the MD client → `slipstream` communication
    
    Closest to how exchanges actually distribute data - ITCH, MDP3, PITCH. Add a sequence number to the header, detect gaps, as a stretch goal (detailed below) you can implement either a snapshot-and-recovery path or A/B feed arbitration (two identical feeds, take whichever packet arrives first, drop the duplicate).
    
    **Cost:** you now own reliability through redundancy, and there is no `recv()` that hands you an ordered stream.
    
    <aside>
    💬
    
    **Interview payoff:** if you can explain A/B arbitration and gap fill from having built it, you are ahead of most candidates with three years of experience.
    
    </aside>
    

---

# 8. Constraints

- **C++20 minimum.** C++23 welcome. Build with CMake; must compile clean on Linux with `-Wall -Wextra -Wpedantic` under either GCC or Clang.
- **No third-party libraries in the core** (codec, feed handler, book, VWAP, strategy, order path). A test framework is fine. A input parsing library is also fine. gRPC/protobuf are permitted *only* inside the M4 transport module.
- **No floating point in accumulation.** Fixed-point integers throughout. Float is acceptable only for display formatting.
- **[Optional] No heap allocation in the steady-state hot path.** Allocate at startup. If you allocate per tick, say so in the README and explain why.
- Everything in `main()` is a red flag. So is a 900-line `God` class.

---

# 9. Scoring Rubric

| Dimension | Weight | What "strong" looks like |
| --- | --- | --- |
| Wire correctness | 20% | Handles split frames, partial reads, malformed input, and mid-stream disconnects without corrupting state |
| Data structures | 20% | Rolling window is O(1) amortized with a defended choice; accumulator overflow is bounded or proven |
| Order state machine | 20% | `in_flight` tracked (optional); timeouts released and reconciled (optional); no double-counting; never exceeds maximum or participation cap |
| Latency discipline | 15% | Allocation-free hot path, measured p50/p99/p99.9, and a candidate explanation for the tail |
| Design and testability | 15% | Transport abstracted; strategy unit-testable without a socket; scenario files cover adversarial cases |
| README and reasoning | 10% | Assumptions listed, known issues listed honestly, tradeoffs named |

The README carries more weight than its 10% suggests. Documentation and communication is critical. Interviewers read it before they read your code, and "known issues: my VWAP window degrades to O(n) under a burst of same-nanosecond prints, here's why I accepted that" reads as introspective and mature. 

---

# 10. Places to watch out.

A couple of high-level points on where you might trip-u[.

- [ ]  Assuming one `recv()` returns exactly one message.
- [ ]  Tumbling window instead of rolling - VWAP resets and the strategy fires on a two-print benchmark.

**Tumbling (wrong):** you bucket time into fixed, non-overlapping 30-second blocks. At t=30.000s you throw away everything and start a fresh accumulator.

```cpp
if (now - window_start >= window_ms) {
    sum_px_qty = 0;      // <- the reset
    sum_qty     = 0;
    window_start = now;
}
sum_px_qty += px * qty;
sum_qty    += qty;
```

**Rolling / sliding (right):** the window is always the last 30 seconds *relative to right now*. It moves continuously. Nothing ever resets, old prints are evicted one at a time from the front as they age past now − vwap_window_ms.

```cpp
while (!prints.empty() && prints.front().ts_ns < now - window_ns) {
    sum_px_qty -= prints.front().px * prints.front().qty;
    sum_qty    -= prints.front().qty;
    prints.pop_front();
}
```

- [ ]  64-bit accumulator (e.g. for storing your quantity) overflow on a long session with large notional.
- [ ]  Sending `NewOrder` trades messages before warm-up completes.
- [ ]  `struct` layouts that rely on default padding and break the moment a compiler flag changes.
- [ ]  Reading the symbol field as a null-terminated string when it is exactly 12 characters.

---

# 11. Stretch Goals

You’ll definitely need more details for these. These are general future directions to take your project. Ask your coach or CJ for more once you get here.

- [ ]  A/B feed arbitration across two multicast feeds.
- [ ]  Compute `in_flight` quantities, timeouts and reconciliation, sent from the OE client the `slipstream`.
- [ ]  Snapshot + incremental recovery after a detected gap.
- [ ]  Multi-symbol with per-symbol sharding across threads.
- [ ]  A pre-trade risk gate and kill switch - max notional, max order rate, fat-finger price check.
- [ ]  Backpressure handling in the OE client (exponential backoff) when the `slipstream` throttles (`reason_code = 4`).
- [ ]  `perf` profile and a flame graph in the README showing where the time actually goes.

---

# 12. Submission

Submit a repository containing:

1. Source code, `CMakeLists.txt`, and a one-command build.
2. `README.md`: design overview, assumptions, known issues, latency numbers, and the transport comparison.
3. Tests, with `gtest` or an alternative. Data can be written in-code or sourced from files.
4. The benchmark harness and how to reproduce your numbers.

Post the link to this repository in your private Discord channel with your coach. Your coach will review it the way an interviewer would.

---

## Coach notes - internal
