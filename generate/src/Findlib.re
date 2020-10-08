open Helper;

type findlib = {
  target: [ | `Host | `Target(string)],
  path: list(string),
  destdir: string,
  ocaml_install: string,
};
let serialize_findlib = findlib => {
  let append_prefix_to_ocamlpath = path => {
    switch (findlib.target) {
    | `Host => path
    | `Target(toolchain) =>
      Filename.dirname(path)
      ++ "/"
      ++ toolchain
      ++ "-sysroot/"
      ++ Filename.basename(path)
    };
  };
  let path =
    findlib.path
    |> List.map(append_prefix_to_ocamlpath)
    |> String.concat(":");
  [
    ("path", path),
    ("destdir", findlib.destdir),
    ("stdlib", findlib.ocaml_install ++ "/lib/ocaml"),
    ("ocamlc", findlib.ocaml_install ++ "/bin/ocamlc"),
    ("ocamlopt", findlib.ocaml_install ++ "/bin/ocamlopt"),
    ("ocamlcp", findlib.ocaml_install ++ "/bin/ocamlcp"),
    ("ocamlmklib", findlib.ocaml_install ++ "/bin/ocamlmklib"),
    ("ocamlmktop", findlib.ocaml_install ++ "/bin/ocamlmktop"),
    // TODO: probably ocamldoc isn't available
    ("ocamldoc", findlib.ocaml_install ++ "/bin/ocamldoc"),
    ("ocamldep", findlib.ocaml_install ++ "/bin/ocamldep"),
  ]
  |> List.map(((key, value)) => {
       let key =
         switch (findlib.target) {
         | `Host => key
         | `Target(name) => key ++ "(" ++ name ++ ")"
         };
       key ++ " = " ++ "\"" ++ value ++ "\"";
     })
  |> String.concat("\n");
};

let run_command_at_env = (env, cmd) => {
  let env =
    env
    |> List.filter_map(
         fun
         | Esy.Set(key, value) =>
           Some("export " ++ key ++ "=\"" ++ value ++ "\"")
         | Esy.Unset(key) => Some("unset " ++ key)
         | _ => None,
       );
  let command = String.concat("\n", env @ [cmd]);
  Lib.exec(command);
};
let get_variable_value_at_env = (env, var) => {
  // TODO: multiple variable per command
  let+await output = run_command_at_env(env, "echo ${" ++ var ++ "}");
  output |> String.trim;
};
let get_ocamlpath = build_env => {
  // TODO: filter only the last one
  let env = build_env |> List.filter((!=)(Esy.Unset("OCAMLPATH")));
  let+await ocamlpath = get_variable_value_at_env(env, "OCAMLPATH");
  ocamlpath |> String.split_on_char(':');
};

let find_ocaml_install = (build_env, target) => {
  switch (target) {
  | `Host =>
    let+await ocamlrun_path =
      run_command_at_env(build_env, "dirname $(dirname $(which ocamlrun))");
    ocamlrun_path |> String.trim;
  | `Target(_) => get_variable_value_at_env(build_env, "ESY_TOOLCHAIN_OCAML")
  };
};

let emit = (esy, target, name) => {
  let target = `Target(target);

  let.await build_env = esy.Esy.build_env(name);
  let.await path = get_ocamlpath(build_env);
  let.await destdir =
    get_variable_value_at_env(build_env, "OCAMLFIND_DESTDIR");

  let.await host_findlib = {
    let target = `Host;
    let+await ocaml_install = find_ocaml_install(build_env, target);
    serialize_findlib({target, path, destdir, ocaml_install});
  };
  let.await target_findlib = {
    let+await ocaml_install = find_ocaml_install(build_env, target);
    serialize_findlib({target, path, destdir, ocaml_install});
  };
  await((host_findlib, target_findlib));
};
