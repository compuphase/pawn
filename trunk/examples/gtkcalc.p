/* A demo GTK application in Pawn, using gtk-server and the "process control"
 * module for Pawn. This example also illustrates the use of (communicating)
 * finite state machines.
 */
#include <process>
#include <string>

enum Btn
    {
    Btn0,    Btn1,    Btn2,
    Btn3,    Btn4,    Btn5,
    Btn6,    Btn7,    Btn8,
    Btn9,    BtnDot,  BtnC,
    BtnCE,   BtnPlus, BtnMin,
    BtnMul,  BtnDiv,  BtnEqual,
    BtnNone,
    }

static entry    /* GTK "entry" widget */
static numberstring[40 char]
static accum
static Btn:pending_op

adddigit(digit, bool:reset = false)
    {
    new command[80 char], charstr[2 char]
    charstr{0} = (0 <= digit <= 9) ? digit + '0' : digit
    if (reset)
        numberstring[0] = EOS
    strcat numberstring, charstr
    strformat command, _, true, "gtk_entry_set_text %d %s", entry, numberstring
    GTK(command)
    }

displayresult(value)
    {
    new command[80 char]
    valstr numberstring, value, true
    strformat command, _, true, "gtk_entry_set_text %d %s", entry, numberstring
    GTK(command)
    }


resetentry(digit) <number:negated>
    {
    adddigit '-', .reset = true
    adddigit digit
    }

resetentry(digit) <>
    {
    adddigit digit, .reset = true
    }


event_0() <number:zero, number:negated>
    {
    resetentry 0
    }

event_0() <number:int, number:frac>
    {
    adddigit 0
    }

event_1_9(Btn:idx) <number:zero, number:negated>
    {
    resetentry _:(idx - Btn0)
    state number:int
    }

event_1_9(Btn:idx) <number:int, number:frac>
    {
    adddigit _:(idx - Btn0)
    }

event_dot() <number:zero, number:negated>
    {
    resetentry 0
    adddigit '.'
    state number:frac
    }

event_dot() <number:int>
    {
    adddigit '.'
    state number:frac
    }

event_dot() <number:frac>
    {
    /* reject entry */
    }

event_minus() <number:zero, number:negated>
    {
    state number:negated
    resetentry 0
    }

event_minus() <>
    {
    event_oper BtnMin   /* forward to the "calc" automaton */
    }


/* helper function for the calculator automaton */
performoperation()
    {
    /* get the other operand, perform the operation */
    new val = strval(numberstring)
    switch (pending_op)
        {
        case BtnPlus:   accum += val
        case BtnMin:    accum -= val
        case BtnMul:    accum *= val
        case BtnDiv:    accum = (val == 0) ? 0 : accum / val
        }
    displayresult accum
    }

event_oper(Btn:idx) <calc:idle>
    {
    /* save operand and operator */
    accum = strval(numberstring)
    pending_op = idx

    state number:zero
    state calc:pending
    }

event_oper(Btn:idx) <calc:pending>
    {
    performoperation            /* do the pending operation */
    pending_op = idx            /* save the operator for the next operation */
    state number:zero
    }

event_equal() <calc:idle>
    {
    /* ignore */
    }

event_equal() <calc:pending>
    {
    performoperation            /* do the pending operation */
    state calc:idle             /* reset the calculator */
    state number:zero
    }


event_C()
    {
    state calc:idle
    state number:zero
    resetentry 0
    }

event_CE()
    {
    state number:zero
    resetentry 0
    }


main()
    {
    if (!procexec("gtk-server stdin") && !procexec("gtk-server.exe stdin"))
        fatal "unable to launch gtk-server"

    /* make a window */
    GTK("gtk_init NULL NULL")
    new win = GTK("gtk_window_new 0")
    GTK("gtk_window_set_title %d \"Pawn calculator\"", win)
    GTK("gtk_widget_set_usize %d 200 200", win)

    /* add a table (align the other controls) */
    new table = GTK("gtk_table_new 50 50 1")
    GTK("gtk_container_add %d %d", win, table)

    /* the number entry */
    entry = GTK("gtk_entry_new")
    GTK("gtk_table_attach_defaults %d %d 1 49 1 9", table, entry)

    /* the key pad */
    new buttons[Btn]
    buttons[BtnDot] = GTK("gtk_button_new_with_label .")
    GTK("gtk_table_attach_defaults %d %d 21 29 41 49", table, buttons[BtnDot])
    buttons[Btn0] = GTK("gtk_button_new_with_label 0")
    GTK("gtk_table_attach_defaults %d %d 1 19 41 49", table, buttons[Btn0])
    buttons[Btn1] = GTK("gtk_button_new_with_label 1")
    GTK("gtk_table_attach_defaults %d %d 1 9 31 39", table, buttons[Btn1])
    buttons[Btn2] = GTK("gtk_button_new_with_label 2")
    GTK("gtk_table_attach_defaults %d %d 11 19 31 39", table, buttons[Btn2])
    buttons[Btn3] = GTK("gtk_button_new_with_label 3")
    GTK("gtk_table_attach_defaults %d %d 21 29 31 39", table, buttons[Btn3])
    buttons[Btn4] = GTK("gtk_button_new_with_label 4")
    GTK("gtk_table_attach_defaults %d %d 1 9 21 29", table, buttons[Btn4])
    buttons[Btn5] = GTK("gtk_button_new_with_label 5")
    GTK("gtk_table_attach_defaults %d %d 11 19 21 29", table, buttons[Btn5])
    buttons[Btn6] = GTK("gtk_button_new_with_label 6")
    GTK("gtk_table_attach_defaults %d %d 21 29 21 29", table, buttons[Btn6])
    buttons[Btn7] = GTK("gtk_button_new_with_label 7")
    GTK("gtk_table_attach_defaults %d %d 1 9 11 19", table, buttons[Btn7])
    buttons[Btn8] = GTK("gtk_button_new_with_label 8")
    GTK("gtk_table_attach_defaults %d %d 11 19 11 19", table, buttons[Btn8])
    buttons[Btn9] = GTK("gtk_button_new_with_label 9")
    GTK("gtk_table_attach_defaults %d %d 21 29 11 19", table, buttons[Btn9])

    buttons[BtnC] = GTK("gtk_button_new_with_label C")
    GTK("gtk_table_attach_defaults %d %d 31 39 11 19", table, buttons[BtnC])
    buttons[BtnCE] = GTK("gtk_button_new_with_label CE")
    GTK("gtk_table_attach_defaults %d %d 41 49 11 19", table, buttons[BtnCE])
    buttons[BtnPlus] = GTK("gtk_button_new_with_label +")
    GTK("gtk_table_attach_defaults %d %d 31 39 21 29", table, buttons[BtnPlus])
    buttons[BtnMin] = GTK("gtk_button_new_with_label -")
    GTK("gtk_table_attach_defaults %d %d 41 49 21 29", table, buttons[BtnMin])
    buttons[BtnMul] = GTK("gtk_button_new_with_label x")
    GTK("gtk_table_attach_defaults %d %d 31 39 31 39", table, buttons[BtnMul])
    buttons[BtnDiv] = GTK("gtk_button_new_with_label /")
    GTK("gtk_table_attach_defaults %d %d 41 49 31 39", table, buttons[BtnDiv])
    buttons[BtnEqual] = GTK("gtk_button_new_with_label =")
    GTK("gtk_table_attach_defaults %d %d 31 49 41 49", table, buttons[BtnEqual])

    /* initialize automata */
    state number:zero
    state calc:idle

    /* wait for events, and dispatch them */
    GTK("gtk_widget_show_all %d", win)

    resetentry 0
    new event
    new Btn:idx
    do
        {
        event = GTK("gtk_server_callback wait")

        /* find the button matching the event, generate the event */
        for (idx = Btn0; idx < BtnNone && buttons[idx] != event; idx++)
          {}
        switch (idx)
            {
            case Btn0:
                event_0
            case Btn1 .. Btn9:
                event_1_9 idx
            case BtnDot:
                event_dot
            case BtnMin:
                event_minus
            case BtnPlus, BtnMul, BtnDiv:
                event_oper idx
            case BtnEqual:
                event_equal
            case BtnC:
                event_C
            case BtnCE:
                event_CE
            }
        }
    while (event != win);

    /* direct call, because we must not wait for a reply on this command */
    procwrite "gtk_exit 0", true
    }

GTK(const format[], ...)
    {
    new command[256 char]
    switch (numargs())
        {
        case 1:
            strpack command, format
        case 2:
            strformat command, _, true, format, getarg(1)
        case 3:
            strformat command, _, true, format, getarg(1), getarg(2)
        case 4:
            strformat command, _, true, format, getarg(1), getarg(2), getarg(3)
        case 5:
            strformat command, _, true, format, getarg(1), getarg(2), getarg(3), getarg(4)
        }
    procwrite command, true

    new reply[30]
    procread reply, .striplf=true
    if (strcmp(reply, "ok") == 0)
        return true
    return strval(reply)
    }

fatal(const message[])
    {
    printf "FATAL: %s\n", message
    exit
    }
