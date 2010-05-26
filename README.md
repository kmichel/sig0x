# sig0x
A typesafe callback system for standard C++0x - a bit like libsigc++

[Libsigc++](http://libsigc.sourceforge.net/) has much more features,
such as non-void return types, marshallers and trackable base class for
automatic disconnection. Sig0x is less 300loc, probably faster to parse and
compile, and mostly just an experiment :)

## Usage

### Define a signal
    sig0x::Slot<int, float> event;

### Bind to a function, simple variant
    void func(int a, float b);

    event.bind(&func);

### Bind to a function, 'fast' variant when function has external linkage
    void func(int a, float b);
    static void static_func(int a, float b);

    event.bind<&func>();
    event.bind<&static_func>(); // This won't work

### Bind to a lambda
    event.bind([](int a, float b){
        std::cout << "recieved two numbers: " << a << "," << b << std::endl; });

### Bind to a method
    struct Object {
        void func(int a, float b);
    };
    Object o;

    event.bind(o, &Object::func);
    // Or the 'fast' variant
    event.with(o).bind<&Object::func>();

### Fire event
    event.fire(42, 3.14f);
    // Or the simple variant
    event(42, 3.14f);

### Disconnect from an event
    auto link = event.bind(func);

    link.release();

### Disconnect from an event, RAII-style
    {
        sig0x::Slot<int, float>::AutoLink autolink = event.bind(func);
    } // link released here

This one is particularly useful if used as a member of a class
to disconnect event recievers when 'this' is deleted.

    struct Object {
        sig0x::Slot<int, float>::AutoLink autolink;
        void func(int a, float b);
    };

    sig0x::Slot<int, float> event;
    {
        std::unique_ptr<Object> o(new Object());

        o->autolink = event.with(o).bind(&Object::func);
    } // o is deleted

    // it's safe, no dangling link between event and the deleted o
    event.fire(42, 3.14f);
