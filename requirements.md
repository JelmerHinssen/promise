The system must provide a way to write asynchronous code.  

The following two segments of code must have the same semantics.  
```
co_await task();
second_task();
```
```
task().then([](){
    second_task();
});
```

Preferably you could leave out the co_await in the example before. To run without blocking you would write 
```
task().run_asynchronously();
```
The question is then if `task().then()[](){second_task();};` blocks or not.

Anyway I don't think this is possible. 



# Coroutines in JEP
Coroutines in JEP essentially form a stack of callstacks.

All syntax mentioned below is preliminary and subject to change.

A coroutine is a separate callstack together.
A coroutine is created using `run_in_coroutine(f);`
This creates a new coroutine with the current coroutine set as parent coroutine. This means that a new callstack is created and f is called in this callstack. When `f` finishes or blocks, the current coroutine/function is resumed.

A coroutine blocks when `await condition_variable;` is encountered. `condition_variable` now holds a reference to the current coroutine. The callstack of the current coroutine is saved and control flow is handed the parent coroutine. The coroutine that blocks now no longer has a parent. If the current coroutine doesn't have a parent, then the caller of the script is resumed. This may be a runtime error depending on the program that runs the script.


When `condition_variable.notify();` is called, the current coroutine becomes the parent of the coroutine to which condition_variable belongs and that coroutine is resumed. This means that the callstack of the coroutine is restored and the line after `await condition_variable;` is now executed.

The main difference between a thread and a coroutine is that there is always only one coroutine executed at the same time (per thread), whereas multiple threads are executed at the same time (technically interwoven, which is handled by the OS). Advantage is that you have control over when execution can stop, so you don't need thread synchronization tools (except for contidtion variables).

## yield
A statement of the form `yield;` or `yield expr;` also blocks the current coroutine, similarly to `await;`.
The call to `run_in_coroutine(f);` returns a reference to the created coroutine. Using this reference the coroutine can also be resumed, but only if the coroutine was blocked using `yield`. If the reference to the coroutine isn't captured, `yield` should do nothing.
If a value was yielded, it can be obtained from the reference to the coroutine. If `f` returns a value it can also be obtained from this reference.
`final_yield` can be used to signal that the coroutine should/cannot be resumed anymore. 

## Try-catch
Blocking a coroutine involves a kind of stack unwinding. This indicates that it can be used to handle exceptions.
`throw e;` is the same as `final_yield Exception(e);` and 
```
try {
    f();
} catch (Exception e) {
    g(e);
} finally {
    h();
}
```
Is the same as
```
cr = run_in_coroutine(f);
check:
if (cr.yielded) {
    v = cr.yield_value;
    if (v instanceOf Exception) {
        g(v);
    } else {
        yield v;
        cr.resume();
        goto check;
    }
} else if (cr.waiting_for_cv) {
    oldcv = cr.condition_variable;
    cv = ConditionVariable();
    cr.condition_variable = cv;
    await cv;
    cv.notify();
    goto check;
}
// f() finished or threw exception.
h();
```
This definitely isn't zero cost exception handling, since a coroutine is always created, even if no exception is thrown.
Coroutine creation can be expensive, because a callstack needs to be created. But you could reuse the callstack of the current coroutine and only copy it when yielding (but not when final yielding). 

Problem with this code is that destructors aren't called, since this actually is not stack unwinding.
Possible (bad) solution create coroutine for every object that needs a nontrivial destructor, i.e. 
```
try {
    Obj obj(args);
    do_stuf(obj);
} finally {
    ~obj();
}
```
This way stack unwinding is implemented in the stack of callstacks, but not within the callstacks themselves.
Yielding can also be expensive when lots of objects need destructors.


It looks like this is similar to lua coroutines.

# Threads in JEP
A thread is a coroutine that is periodically interrupted by the runtime. This is as if every so often `yield` is inserted and the runtime is responsible for resuming the coroutine again. It could also just be a coroutine executed on an OS thread.

# Coroutines in C++
The behaviour described above is all very similar to that of C++ coroutines, with the big exception that a function is a coroutine if it uses co_return, co_await or co_yield and that these can only be used in the main body of the coroutine, not in nested calls. The implementation that I had in mind is very similar to javascript async functions.
I think there are three options:
- Implement JEP-style coroutines in C++. This should be possible, but is very difficult as it requires the manual creation of a callstack. This must be executable memory, so it is probably not cross platform.
- Implement javascript style async functions. This is what I already kind of have. This requires that every function that blocks is a coroutine function and every function that calls such a function in a synchronous way (which almost all calls are) are also coroutine functions. This is anoying, because this can change later. You could just make every function a coroutine function, but that gives quite some overhead.
- Try to implement JEP-style coroutine behaviour using C++ coroutines and exceptions. An await in a nested function would be `throw Await(condition_variable)`. The main function body has something like. 
```
try {
    f();
} catch (Await& e) {
    co_await e.condition_variable;
    e.resume();
}
```
The problem is that I don't know how you would resume after that. I don't think it works, because stack-unwinding has already happened, so you can't resume from the point where you want to. Somehow the entire state should be stored in the Await exception, so this requires the same amount of effort as the first option. The first option is definitely better than this one.

For now I will go for the second option.

# Hooks in JEP
It should be possible to hook into functions. Intended usage for this is in unit testing to count function calls. Another usage is to separate animations from logic. Every function has lists preHook and postHook. You can also add filters to prevent a function from being executed
```
f(args) {
    for (filter : filters) {if (filter(args)) return;}
    for (hook : preHooks) {hook(args);}
    actual_f(args);
    for (hook : postHooks) {hook(args);}
}
```

I'm not sure how good of an idea these filters are. The idea of having filters is motivated by the idea of skipping animations when an action isn't allowed. I guess you could also do this like 
```
void f() {
    if (!allowed) return;
    g();
}
```
and hooking into `g`. But this requires exposing both `f` and `g` as a public api. Another possibility would be to add keyword to allow something like
```
void f( {
    if (!allowed) return;
    prehook_here;
    g();
    posthook_here;
    h();
})
```
Although this reduces the separation of logic and animation. Ideally the function should know nothing about its hooks.

I want all functions to be hookable, but this may require too much overhead: Two lists for every function and iterating over them for every function call. In practise, most functions will not be hooked into and more than one hook is rare. During the optimization phase it should be possible to prove for most functions that it is not necessary and leave it out of the code generation. For named functions this shouldn't be to hard, but for functions that are passed along to other functions and variables this can be difficult.

## Hooks in C++
Only coroutines can be hooked. This can be implemented as
```
f(args) {
    for (filter : filters) {if (filter(args)) co_return;}
    for (hook : preHooks) {co_await hook(args);}
    co_await actual_f(args);
    for (hook : postHooks) {co_await hook(args);}
}
```
This has the caveat that hooks must be coroutines, but I think that is what I want, because I want animations to block the main logic.
