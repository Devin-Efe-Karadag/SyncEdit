class Crdt {
  std::map<Id, Element> nodes;

  std::map<Id, Operation> pending;

  std::map<Id, Operation> history;

  std::multimap<Id, Id> waiting;
  Vector versions;

  size_t contiguous = 0;
  OrderIndex order;
  void drain(Operation first);
  void remember(const Operation &o);

public:
