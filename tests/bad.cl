class Broken1 {
  x : Int <- ;

  good() : Int {
    1
  };
};

class Broken2 {
  badMethod(x Int) : Int {
    x
  };

  okMethod() : String {
    "ok"
  };
};

class Broken3 {
  main() : Object {
    {
      if true then 1 fi;

      let x : Int <- 5, y : String <- in x;

      while true loop pool;

      case 5 of
        a Int => a;
      esac;

      1 + ;
    }
  };
};

class Main {
  main() : Int {
    0
  };
};