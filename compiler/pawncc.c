/*  Pawn compiler driver
 *
 *  Pawn is a scripting language system consisting of a compiler and an
 *  abstract machine, for building and running programs in the Pawn language.
 *
 *  Copyright (c) ITB CompuPhase, 2006-2013
 *
 *  Licensed under the Apache License, Version 2.0 (the "License"); you may not
 *  use this file except in compliance with the License. You may obtain a copy
 *  of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 *  WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 *  License for the specific language governing permissions and limitations
 *  under the License.
 *
 *  Version: $Id: pawncc.c 4983 2013-10-21 07:32:57Z  $
 */
#include "sc.h"

int main(int argc, char *argv[])
{
  return pc_compile(argc,argv);
}
