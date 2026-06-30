(*
  BAD_HIERARCHY.CL — sintaksno validan, ali graf nasljeđivanja je neispravan.
  Ovaj test je zaseban jer semant.c namjerno prekida nakon gresaka grafa.
*)

class A inherits B { };
class B inherits C { };
class C inherits A { };

(* Zabranjeno nasljeđivanje od osnovne klase. *)
class BadInt inherits Int { };
class BadString inherits String { };
class BadBool inherits Bool { };

(* Zabranjeni/nepostojeći roditelji. *)
class BadSelfType inherits SELF_TYPE { };
class UnknownParent inherits DoesNotExist { };

(* Zabranjeno redefinisanje osnovne klase. *)
class Object { };

class Main {
  main() : Object { 0 };
};
