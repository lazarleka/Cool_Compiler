class Helper {
  value : Int <- 10;

  getValue() : Int {
    value
  };
  

  add(x : Int, y : Int) : Int {
    x + y
  };
};

class Main inherits IO {
  x : Int;
  text : String <- "hello";

  main() : Object {
    {
      
      x <- 5;
      (*repit
         x <- x + 1
        until x = 10;*)
      out_string("Start\n");
      out_int(x + 2 * 3);

      if x < 10 then
        out_string("small\n")
      else
        out_string("big\n")
      fi;

      while x < 8 loop {
        x <- x + 1;
        out_int(x);
      } pool;

      let a : Int <- 1, b : Int <- 2, c : String <- "test" in {
        out_int(a + b);
        out_string(c);
      };

      case x of
        i : Int => out_int(i);
        o : Object => out_string("object\n");
      esac;

      out_string("done\n");

      self;
    }
  };
};