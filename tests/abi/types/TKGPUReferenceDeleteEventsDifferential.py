"""Compare public event queue order, filters, markers and deletion predicates."""
import argparse
import ctypes as c
import random


class Event(c.Structure):
    _fields_ = [("proc", c.c_void_p), ("next", c.c_void_p),
                ("number", c.c_int), ("mask", c.c_int)]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("candidate")
    p.add_argument("--reference", default="libtcl8.6.so")
    args = p.parse_args()
    libs = [c.CDLL(args.reference), c.CDLL(args.candidate)]
    pointer, integer = c.c_void_p, c.c_int
    handler_type = c.CFUNCTYPE(integer, pointer, integer)
    delete_type = c.CFUNCTYPE(integer, pointer, pointer)
    consumed, visited = [[], []], [[], []]
    handlers, predicates = [], []

    def make_handler(index):
        def handle(address, flags):
            event = Event.from_address(address)
            if not flags & event.mask:
                return 0
            consumed[index].append(event.number)
            return 1
        return handler_type(handle)

    def make_predicate(index):
        def delete(address, data):
            event = Event.from_address(address)
            visited[index].append(event.number)
            return event.number % (int(data or 0) + 2) == 0
        return delete_type(delete)

    for index, lib in enumerate(libs):
        lib.Tcl_FindExecutable.argtypes = [c.c_char_p]
        lib.Tcl_FindExecutable(b"event-differential")
        lib.Tcl_CreateInterp.restype = pointer
        lib.Tcl_DeleteInterp.argtypes = [pointer]
        lib.Tcl_Alloc.argtypes, lib.Tcl_Alloc.restype = [c.c_uint], pointer
        lib.Tcl_QueueEvent.argtypes = [pointer, integer]
        lib.Tcl_ServiceEvent.argtypes, lib.Tcl_ServiceEvent.restype = [integer], integer
        lib.Tcl_DeleteEvents.argtypes = [delete_type, pointer]
        handlers.append(make_handler(index))
        predicates.append(make_predicate(index))
    interps = [lib.Tcl_CreateInterp() for lib in libs]
    rng = random.Random(629)
    try:
        for step in range(20000):
            op = rng.randrange(4)
            position = rng.randrange(3)
            mask = rng.choice((4, 8, 16, 32))
            divisor = rng.randrange(4)
            results = []
            for index, lib in enumerate(libs):
                if op < 2:
                    address = lib.Tcl_Alloc(c.sizeof(Event))
                    event = Event.from_address(address)
                    event.proc = c.cast(handlers[index], pointer).value
                    event.next, event.number, event.mask = None, step, mask
                    lib.Tcl_QueueEvent(address, position)
                elif op == 2:
                    lib.Tcl_DeleteEvents(predicates[index], pointer(divisor))
                else:
                    results.append(lib.Tcl_ServiceEvent(mask))
            assert len(results) != 2 or results[0] == results[1], (step, results)
            assert consumed[0] == consumed[1], (step, "service order")
            assert visited[0] == visited[1], (step, "delete order")
        for lib in libs:
            while lib.Tcl_ServiceEvent(0):
                pass
        assert consumed[0] == consumed[1], "final queue order"
    finally:
        for lib, interp in zip(libs, interps):
            lib.Tcl_DeleteInterp(interp)
    print("20000 event queue/deletion/filter/marker sequences matched stock Tcl")


if __name__ == "__main__":
    main()
