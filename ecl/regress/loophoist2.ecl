/*##############################################################################

    Copyright (C) 2011 HPCC Systems.

    All rights reserved. This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
############################################################################## */


namesRecord :=
            RECORD
string20        surname;
string10        forename;
integer2        age := 25;
            END;

namesTable := dataset('x',namesRecord,FLAT);

otherTable := dataset('other',namesRecord,FLAT);


processLoop(dataset(namesRecord) in, unsigned c) := FUNCTION

    x := otherTable(LENGTH(TRIM(surname)) > 1);
    x2 := dedup(x, surname, all);

    //Use x2 from a child query - so it IS force to a single node
    y := JOIN(in, x2, LEFT.surname = RIGHT.surname and LEFT.surname != x2[c].surname);

    RETURN y;
END;


ds1 := LOOP(namesTable, 100, processLoop(ROWS(LEFT), COUNTER));
output(ds1);
