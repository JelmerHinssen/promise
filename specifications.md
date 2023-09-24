# Types
The different types of objects are
- `Coroutine` represents the actual coroutine, i.e. the function that is being executed and its callstack.
- `CoroutineHandle` is a pointer to a `Coroutine`. Once all handles to a coroutine are destroyed, the coroutine is also destroyed. There can be multiple handles to the same coroutine, but only one main handle. Every handle can read from a coroutine, but only the main handle can change it.
- `Promise<T>` is the main outward facing interface of a coroutine. Contains/is a handle to a coroutine. The promise stores the return/yield values.
- `SuspensionPoint<S>` condition that can be waited for. Contains/is a handle to a coroutine. Allows to synchronize between a coroutine and other functions.

# Coroutine stack
A coroutine contains a handle to the coroutine it is called by and a reference to the 

# Creating coroutines
1. An asynchronous function with return type `T` is declared as  
```
Promise<T> foo();
```
2. `T` can be any type that a normal function can return this includes `void` and reference types but excludes arrays.
3. The function body must contain `co_return`, `co_await` or `co_yield` to be an actual asynchronous function.
4. A call to an asynchronous function creates a coroutine, but does not start it.
5. The `Promise<T>` object contains a handle to the created coroutine, which we denote by `.coroutine`. This is the main handle when first created.
8. You can chain coroutines using:  
   - `Promise<S> Promise<T>::then(S(T))` 
   - `Promise<S> Promise<T>::then(Promise<S>(T))`  
  If `T` is `void` then the argument type is `S()` or `Promise<S>()`. If `S = T` then the returned promise may be a reference to the original promise.
  Let `c = a.then(f)`. Executing `c.coroutine` is equivalent to first executing `a.coroutine` and then `f()` or `f().coroutine`.



8. A coroutine is started using `void Promise<T>::start()`. This is only allowed if the underlying coroutine has not been started before.

# Suspending
- `S operator co_await(Promise<S>&&)` The called promise becomes the main handle of this coroutine
- `co_yield expr` current calling handle becomes main handle
- `S operator co_await(SuspensionPoint<S>&)` the awaited suspension point becomes the main handle

# Resuming
The main handle can resume the coroutine via `void CoroutineHandle<T>::resume()`. This is only allowed if the handle is the main handle to the coroutine.
10. underlying coroutine is suspended because of a `co_yield`, i.e. `bool Promise<T>::yielded() const noexcept` returns `true`.
11. The yielded value of a coroutine can be obtained using `std::optional<T> Promise<T>::yield_value() const noexcept`
12. The returned value of a coroutine can be obtained using `std::optional<T> Promise<T>::return_value() const noexcept`
13. 
