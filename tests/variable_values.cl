class Main inherits IO {
  x : Int <- 17;
  name : String <- "start";

  main() : Object {
    {
      x <- 15;
      name <- "end";
      true;
    }
  };
};
