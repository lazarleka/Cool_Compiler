(*
  BAD_SYNTAX_LEXER.CL — testira parser/lexer greske.
  Ovdje NE ocekuj semanticku analizu, jer se ona ne pokrece poslije
  leksicke ili sintaksne greske.
*)

class FirstBroken {
  (* Nedostaje izraz poslije <- *)
  x : Int <- ;

  (* Nedostaje ime formalnog parametra. *)
  f( : Int) : Int { 0 };
};

(* Parser treba, koliko moze, da se oporavi i nastavi na sljedecu klasu. *)
class StillValid {
  ok() : Int { 1 };
};

(* Nevažeći karakter treba dati lexical error. *)
#

(* Neupareni zavrsetak komentara. *)
*)
