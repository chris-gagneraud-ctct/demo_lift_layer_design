
## Demo of a Handler/Session/Processor design implementation.

See https://miro.com/app/board/uXjVJxglGx4=/?moveToWidget=3458764648165076252&cot=14

This is a simple CLI demo program that follows the design presented above and demonstrate the asynchronous execution of operations using `std::async`.

```
$ clang++-20 -std=c++17 test.cpp && ./a.out
Usage: Press a command letter, followed by <Enter>
  'b' -> Begin a new session
  'e' -> End current session
  'l' -> Load surface
  'u' -> Update layers
  'g' -> Get preview points
  'c' -> Create design
  'h' -> Print this help message
```

Only `b`, `e`, `l` and `h` are implemented. This shoul'd be enough to evaluate the implementation.
Edge cases can be tested by sending `b`, `e` and `l` commands in quick successive random order.

TODO:
- Test strategy for the Session class (`std::async` to insure stable test results


 
