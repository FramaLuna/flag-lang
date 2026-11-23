# In gdb: source flag-gdb.py. Then a &str prints as text and a &list with its elements; print/r shows the raw fields.
import gdb


class Text:
    def __init__(self, value):
        self.value = value

    def display_hint(self):
        return "string"

    def to_string(self):
        size = int(self.value["len"])
        if size <= 0:
            return ""
        return self.value["data"].lazy_string(encoding="utf-8", length=size)


class Items:
    def __init__(self, value):
        self.value = value

    def display_hint(self):
        return "array"

    def to_string(self):
        return None if int(self.value["len"]) > 0 else "{}"

    def children(self):
        data = self.value["data"]
        for i in range(max(int(self.value["len"]), 0)):
            item = (data + i).dereference()
            try:
                item.fetch_lazy()
            except gdb.MemoryError:  # a variable that has no value yet must not stop info locals
                yield "[%d]" % i, "<cannot read this memory>"
                return
            yield "[%d]" % i, item


def printer(value):
    name = value.type.strip_typedefs().tag or ""
    if name in ("&str", "&strview"):
        return Text(value)
    if name.startswith(("&list[", "&listview[")):
        return Items(value)


gdb.pretty_printers.append(printer)
