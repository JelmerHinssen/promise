# Types
The different types of objects are
- `Coroutine` represents the actual coroutine, i.e. the function that is being executed and its callstack.
- `CoroutineHandle` is a pointer to a `Coroutine`. Once all handles to a coroutine are destroyed, the coroutine is also destroyed. There can be multiple handles to the same coroutine, but only one main handle. Every handle can read from a coroutine, but only the main handle can change it.
- `Promise<R, Y>` is the main outward facing interface of a coroutine. Contains/is a handle to a coroutine. The promise stores the return/yield values of type `R` and `Y` respectively.
- `SuspensionPoint<T>` condition that can be waited for. Contains/is a handle to a coroutine. Allows to pass an object of type `T` to the coroutine.

# Coroutine stack
Every coroutine has members
`Coroutine* parent`, `CoroutineHandle* yield_owner` and `yield_type* yield_object`.
Initially `parent = nullptr`, `yield_owner` is the handle of the newly created `Promise` and `yield_object` points to the yield object in this `Promise`.

# Creating coroutines
1. An asynchronous function with return type `R` and yield type `Y` is declared as  
```
Promise<R, Y> foo();
```
2. `R` and `T` can be any type that a normal function can return. This includes `void` and reference types but excludes arrays.
3. The function body must contain `co_return`, `co_await` or `co_yield` to be an actual asynchronous function.
4. A call to an asynchronous function creates a coroutine, but does not start it.
5. The `Promise<R, Y>` object contains a handle to the created coroutine, which we denote by `.coroutine`. This is the main handle when first created.
8. You can chain coroutines using:  
   - `Promise<S, Y> Promise<R, Y>::then(S(R))` 
   - `Promise<R2, Y2> Promise<R, Y>::then(Promise<R2, Y2>(R))` (`Y2` and `Y` must be compatible)
  If `R` is `void` then the argument type is `R()` or `Promise<R2, Y2>()`. If `R = T` then the returned promise may be a reference to the original promise.
  Let `c = a.then(f)`. Executing `c.coroutine` is equivalent to first executing `a.coroutine` and then `f()` or `f().coroutine`.

8. A coroutine is started using `void Promise<T>::start()`. This is only allowed if the underlying coroutine has not been started before.

# Suspending
- `S operator co_await(Promise<S, Y2>&&)`: `called.parent = current`, `called.yield_owner = current->yield_owner`, `called->yield_object = current->yield_object`. The type `Y2` must be convertible to the current yield type. 
- `co_yield expr` the yield owner becomes the main handle. If `yield_owner` is `nullptr` this is either an error or a noop. `expr` must be of type `void` or convertible to the current yield type.
- `S operator co_await(SuspensionPoint<S>&)` the awaited suspension point becomes the main handle

# Resuming
The main handle can resume the coroutine via `void CoroutineHandle::resume()`. This is only allowed if the handle is the main handle to the coroutine.
10. underlying coroutine is suspended because of a `co_yield`, i.e. `bool Promise<R, Y>::yielded() const noexcept` returns `true`.
10. If the underlying coroutine is finished `bool CoroutineHandle::done() const noexcept` returns `true`.
11. The yielded value of a coroutine can be obtained using `optional<Y> Promise<R, Y>::yield_value() const noexcept`
12. The returned value of a coroutine can be obtained using `optional<R> Promise<R, Y>::return_value() const noexcept`
`optional<T>` is the same as `std::optional<T>` except for when `T` is `void` or a reference, in which case it is (a type similar to) `bool` or the corresponding pointer type.

 
