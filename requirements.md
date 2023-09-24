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
In a statically typed language it is hard to yield recursively as the type of expr must be the same as the return type of the main function of the coroutine.
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
Promise<T> f(args) {
    for (filter : filters) {if (filter(args)) co_return;}
    for (hook : preHooks) {co_await hook(args);}
    T result = co_await actual_f(args);
    for (hook : postHooks) {co_await hook(result, args);}
    return result;
}
```
This has the caveat that hooks must be coroutines, but I think that is what I want, because I want animations to block the main logic.

### Problem!
I want to be able to say `f.preHooks += [](){do_something();}`, but `f` itself is just a function.
`Promise::preHooks(f) += [](){do_something();}` is also acceptable. This could be possible. When `f` is called the way coroutines work in C++ is that a promise object is created. If it is possible to get a reference to `f` and its arguments in the constructor of that object, this method is possible. A quick search led me to believe this is not possible. At least not in a portable way. This address must be stored somewhere in memory, but where exactly is implementation defined.

That means that I do need a special Hook<T, Args...> type, just like I have now. This is a function-like object that behaves like a Promise<T>(Args...) function. It's operator() implements the hooks like the code block above.

The annoying thing about this, is that the Hook object needs a name, but the `actual_f` also needs a name, which ideally is the same.

HOOK(name, result, Args...) = 
class name_impl {
    result operator()(Args...);
    friend class Hook<result, Args...>;
};
Hook<result, Args...> name{name_impl};

If it is a member method

HOOK(name, Parent, result, Args...) = 
class name_impl {
    Parent* parent;
    result operator()(Parent, Args...);
    friend class Hook<result, Args...>;
};
friend class name_impl;
Hook<result, Args...> name{name_impl};
Annoying that name_impl::operator() is not actually a member function.
You still have two names. I believe it is impossible to not have one of these problems. You can make the `actual_f` a member function, but then it can never have the same name as the hook.
If you use the pimpl pattern there is a solution as you can make the hook a member of the public type and `actual_f` a member of its Impl class. 
An advantage of a name_impl class is that you can never accidentally call `actual_f` directly, which can happen quite easily using the pimpl pattern.

HOOK(result, Parent, name, Args...) = 
class name : public Hook<result, Args...> {
    Parent* self;
    Promise<result> impl(Args...);
    name(Parent* p): Hook<result, Args...>([this](Args... args){return impl(args...);}), self(p){}
    friend class Parent;
} name{this};
friend class name

Instead of 
Promise<result> name(Args...); // declaration
Promise<result> Parent::name(Args...){} // definition
you get
Hook(result, Parent, name, Args); // declaration
Promise<result> Parent::name::impl(Args...){} // definition

This is not compatible with const functions
CONST_HOOK(result, Parent, name, Args...) = 
class name : public Hook<result, Args...> {
    const Parent* self;
    Promise<result> impl(Args...) const;
    name(const Parent* p): Hook<result, Args...>([this](Args... args){return impl(args...);}), self(p){}
    friend class Parent;
} name{this};
friend class name

Hook must have a const operator() and the non const operator() must be deleted in this case.
For now I will not implement this.

### Problem 2
When a hook or hooked function gets destroyed, copied or moved, unexpected things can happen. For now: delete copy and move constructor of hook and make it the users responsibility to make sure the lifetime of listeners is at least as long as the lifetime of the hook.

### Terminology
In the explanation I have used hook to denote the listeners and hookable function to denote the function they are listening to. But in the code example above i've used the type Hook to denote the hookable function. That needs to be fixed.
I could switch to Observer and Observable or Listener and Event. I do prefer preHooks and postHooks to preListener and postListener, but I prefer Event or Observable to Hookable.
Chat gpt says that Hook point is more common than hookable, but I also cannot get a clear answer to whether a hook refers to an event or a listener.
The problem I have with Event is that it sounds way more deliberate at the point of firing the event, whereas hooking into a function should only be deliberate at the point of the listener. Every function call should silently fire an event.
Observable is a bit better, but I would use that for something like an Observable<int> which fires an event every time the contained value changes
