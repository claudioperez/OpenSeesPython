#include <tcl.h>
#include <stdio.h>

#include <ParticleDomain.h>

int
Tcl_PeridynamicsCommands(ClientData cd, Tcl_Interp* interp,
                         int argc, const char** const argv)
{

  //
  printf("Hello world\n");

  // Loop over the command argument array argv and print each one
  for (int i=0; i<argc; i++)
    printf("%s\n", argv[i]);




  //
  // Parse arguments and create a domain
  //
  // Note that argv[0] holds the name of the command.
  
  // default values
  int totnode = 3;
  int maxfam  = 3;

  // if the size of argv is greater than 1, convert
  // the second element of the array (ie argv[1]) to an integer
  // and store it in totnode
  if (argc > 1) {
    // If this returns TCL_ERROR it means that argv[1] couldnt be
    // parsed as an integer.
    if (Tcl_GetInt(interp, argv[1], &totnode) == TCL_ERROR) {
      printf("ERROR: Couldnt parse argv[1] as an integer\n");
    }
  }

  // Now do the same for the third argument (argv[2])
  if (argc > 2) {
    if (Tcl_GetInt(interp, argv[2], &maxfam) == TCL_ERROR) {
      printf("ERROR: Couldnt parse argv[2] as an integer\n");
    }
  }

  printf("Creating domain with nn=%d, mf=%d\n", totnode, maxfam);
  // Allocate a new domain
  ParticleDomain *domain = new ParticleDomain(totnode, maxfam);

  // Print the domain
  domain->print(0);

  // clean up memory
  delete domain;

  return TCL_OK;
}
