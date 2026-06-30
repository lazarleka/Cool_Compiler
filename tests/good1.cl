(*
  GOOD.CL — veliki validan test za lexer, parser i semantiku.
  Pokriva:
  - ugniježdene komentare, escape stringove i velika/mala slova keyworda
  - klase, nasljeđivanje, override metoda i SELF_TYPE
  - atribute, formale, blok, assignment, let, case, if, while
  - dynamic/self/static dispatch, new, isvoid, not, ~
  - + - * /, <, <= i =
*)

(* spoljašnji komentar
   (* unutrašnji komentar *)
*)

class Parent {
  value : Int <- 10;

  identity(x : Int) : Int { x };

  description() : String { "Parent" };

  copy_me() : SELF_TYPE { self };
};

class Child inherits Parent {
  enabled : Bool <- fAlSe;

  (* Ispravan override: isti broj/formalni tipovi/povratni tip. *)
  description() : String { "Child" };

  bump(x : Int) : Int { x + 1 };

  negate_number(x : Int) : Int { ~x };
};

class Main inherits IO {
  count : Int <- 0;
  word : String <- "COOL\nstring\twith \"quotes\" and \\slash";
  maybe_object : Object;
  parent_ref : Parent <- new Child;
  self_ref : SELF_TYPE <- self;

  add_three(a : Int, b : Int, c : Int) : Int {
    a + b + c
  };

  main() : Object {
    {
      (* self dispatch na naslijeđene IO metode *)
      out_string("=== GOOD TEST START ===\n");
      out_string(word);
      out_string("\n");

      (* aritmetika i precedence: * i / prije + i - *)
      out_int(1 + 2 * 3 / 2 - 1);
      out_string("\n");

      (* dinamički dispatch *)
      out_int((new Child).bump(40));
      out_string("\n");

      (* statički dispatch na metodu roditelja *)
      out_int((new Child)@Parent.identity(7));
      out_string("\n");

      (* chaining dispatch-a i String ugrađene metode *)
      out_string((new Child).description().concat("\n"));
      out_string(word.substr(0, word.length()));
      out_string("\n");
      out_string((new Child).type_name().concat("\n"));

      (* SELF_TYPE kao rezultat metode i new SELF_TYPE *)
      let clone : Main <- new SELF_TYPE in
        clone.out_string("new SELF_TYPE radi\n");

      let as_parent : Parent <- (new Child).copy_me() in
        out_string(as_parent.description().concat(" copied\n"));

      (* assignment i while *)
      while count < 3 loop
        count <- count + 1
      pool;
      out_int(count);
      out_string("\n");

      (* if, isvoid i not *)
      if not isvoid maybe_object then
        out_string("nije void\n")
      else
        out_string("jeste void\n")
      fi;

      (* let: inicijalizacija sljedeće varijable vidi prethodnu;
         kasnije a sakriva ranije a. *)
      let a : Int <- 1, b : Int <- a + 2, a : Int <- b + 3 in {
        out_int(a);
        out_string("\n");
      };

      (* case sa različitim tipovima grana *)
      case parent_ref of
        c : Child => out_string(c.description().concat(" branch Child\n"));
        p : Parent => out_string(p.description().concat(" branch Parent\n"));
        o : Object => out_string("branch Object\n");
      esac;

      (* <=, = i Bool konstante sa miješanim velikim/malim slovima *)
      if (1 <= 2) = tRuE then
        out_string("poredjenja rade\n")
      else
        out_string("greska poredjenja\n")
      fi;

      if not fALSE then
        out_string("not radi\n")
      else
        out_string("greska not\n")
      fi;

      out_int(add_three(1, 2, 3));
      out_string("\n");
      out_int((new Child).negate_number(5));
      out_string("\n=== GOOD TEST END ===\n");

      self;
    }
  };
};
