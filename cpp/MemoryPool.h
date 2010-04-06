#ifndef MEMORYPOOL_H
#define MEMORYPOOL_H

/***** Sample Usage:
 * Pool<Foo> fooPool;
 * Foo* foo = fooPool.request();
 * ...
 * Pool.release(foo);
 *****/

/**
 * PoolWatcher
 *
 * Will be notified when memory events occur.
 * Notified when:
 *      Memory is allocated
 *      Memory is freed
 *      An object is requested
 *      An object is released
 *
 */
class PoolWatcher
{
public:
    /** Memory was allocated from the heap and placed in the pool */
    virtual void onAlloc   (int bytes)=0;
    /** Memory from the pool was freed */
    virtual void onFree    (int bytes)=0;

    /** An object was requested from the pool */
    virtual void onRequest (void* obj)=0;
    /** An object was released back to the pool */
    virtual void onRelease (void* obj)=0;
};

#define CALL_WATCHERS(func)         \
        if (watcher)                \
        {                           \
            Watcher* w = watcher;   \
            do {                    \
                w->watcher->func;   \
                w = w->next;        \
            } while (w);            \
        }

/**
  *
  **/
class Pool
{
public:
    virtual ~Pool () {}

    /** */
    virtual void addWatcher (PoolWatcher* w)=0;

    /** */
    virtual unsigned objectSize () const =0;

    /** */
    virtual void* req ()=0;

    /** */
    virtual void  rel(void*)=0;
};

/**
 * MemoryPool
 *
 * Memory pool for storing unallocated objects.
 * Objects can be requested from the pool and released back to the pool.
 *
 * Provides a more efficient method of constructing and destructing
 * objects, minimizing heap memory allocation/freeing and providing
 * more control over memory management.
 */
template <class C> class MemoryPool : public Pool
{
private:
    struct Node {
        Node* next;
        char data[sizeof(C)];
    }* unused;

    struct Watcher {
        Watcher*     next;
        PoolWatcher* watcher;
    }* watcher;

    /** Allocate num objects */
    void alloc (int num)
    {
        if (num > 0)
        {
            Node* temp;
            CALL_WATCHERS(onAlloc(sizeof(Node) * num));
            while (num-- > 0)
            {
                temp = new Node;
                temp->next = unused;
                unused = temp;
            }
        }
    }

public:
    MemoryPool () : unused(0), watcher(0)
    {
    }

    MemoryPool (int num) : unused(0), watcher(0)
    {
        alloc(num);
    }

    MemoryPool (PoolWatcher* w) : unused(0), watcher(0)
    {
        if (w)
        {
            addWatcher(w);
        }
    }

    MemoryPool (int num, PoolWatcher* w) : unused(0), watcher(0)
    {
        if (w)
        {
            addWatcher(w);
        }
        alloc(num);
    }

    ~MemoryPool ()
    {
        int num = 0;
        Node* temp;
        // Free memory for each unused object
        while (unused != 0)
        {
            temp = unused;
            unused = unused->next;
            delete temp;
            ++num;
        }
        // Notify watchers
        CALL_WATCHERS(onFree(sizeof(Node) * num));

        // Remove watchers (memory for watcher is not freed here)
        Watcher* w;
        while (watcher)
        {
            w = watcher;
            watcher = watcher->next;
            delete w;
        }
    }

    /** Add a watcher to be notified of memory management events */
    void addWatcher (PoolWatcher* w)
    {
        Watcher* watch = new Watcher;
        watch->watcher = w;
        watch->next = watcher;
        watcher = watch;
    }

    unsigned objectSize () const
    {
        return sizeof(C);
    }

    /** Request the construction of a new object */
    C* request ()
    {
        Node* temp;
        if (unused == 0)
        {
            // Allocate a new object
            temp = new Node;
            CALL_WATCHERS(onAlloc(sizeof(Node)));
        } else
        {
            // Unlink the next unused object
            temp = unused;
            unused = unused->next;
        }

        // Notify watchers
        CALL_WATCHERS(onRequest(temp->data));

        // Cnstruct and return object
        return new(temp->data) C;
    }

    /** Release and destruct an onject */
    void release (const C* const  c)
    {
        // Destruct object
        c->~C();
        // Retrieve the node
        Node* temp = (Node*)(((char*)c) - sizeof(Node*));
        // Notify watchers
        CALL_WATCHERS(onRelease(temp->data));
        // Link the object into the list of unused objects
        temp->next = unused;
        unused = temp;
    }

    void* req ()
    {
        return (void*)request();
    }

    void rel(void* o)
    {
        release((const C* const)o);
    }
};