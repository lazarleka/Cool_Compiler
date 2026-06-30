(*
  BAD_SEMANTIC.CL — sintaksno je validan, ali mora dati mnogo
  SEMANTICKIH gresaka. Nemoj ga koristiti za parser recovery; koristi ga
  kada hoces provjeriti semant.c.
*)

class Parent {
  inherited_attr : Int <- 0;

  f(x : Int) : Int { x };
};

class Child inherits Parent {
  (* Zabranjeno: redefinicija naslijeđenog atributa. *)
  inherited_attr : String <- "ne smije";

  (* Zabranjeno: override nema istu signaturu. *)
  f(x : String) : String { x };
};

class Main inherits IO {
  (* Zabranjeno: atribut se ne smije zvati self. *)
  self : Int <- 0;

  (* Zabranjen/nepostojeći deklarisani tip. *)
  unknown_attribute : MissingType;

  (* Ne odgovara Int atributu. *)
  wrong_initializer : Int <- "tekst";

  (* Zabranjeno ime formalnog parametra. *)
  bad_formal(self : Int) : Int { 0 };

  (* Dvostruko ime formalnog parametra. *)
  duplicate_formals(x : Int, x : String) : Int { 0 };

  (* Tijelo nije tipa Int. *)
  wrong_return() : Int { "nije Int" };

  (* main namjerno ima formalni parametar — mora biti bez parametara. *)
  main(argument : Int) : Object {
    {
      (* nedeklarisani identifikator *)
      undeclared_name;

      (* assignment na self i na nepostojeće ime *)
      self <- 1;
      missing_variable <- 1;

      (* pogrešni tipovi operatora *)
      1 + "tekst";
      not 0;
      ~tRuE;
      1 < "tekst";
      1 = "1";

      (* if/while predikati moraju biti Bool *)
      if 1 then 0 else 1 fi;
      while "nije Bool" loop 0 pool;

      (* self ne smije biti let binding; MissingType ne postoji *)
      let self : Int <- 0 in self;
      let x : MissingType <- 0 in x;

      (* case: self je zabranjen, Int grana je duplirana *)
      case 0 of
        self : Int => self;
        a : Int => a;
        b : Int => b;
      esac;

      (* nepostojeća metoda *)
      (new Child).missing_method(1);

      (* Parent.f ocekuje Int, a dobija String *)
      (new Parent).f("pogresan argument");

      (* Child nije podtip od String za static dispatch *)
      (new Child)@String.length();

      (* nepostojeći tip za new *)
      new MissingType;

      0;
    }
  };
};
