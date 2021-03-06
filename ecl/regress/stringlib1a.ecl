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

StringLib := SERVICE : PLUGIN('unknown\\stringlib.dll','STRINGLIB 1.1.14')
  string8 GetDateYYYYMMDD() : c,once,entrypoint='slGetDateYYYYMMDD2';
END;

zzz := StringLib;

loadxml('<AA><BB>1</BB><CC>2</CC></AA>');

#DECLARE (y)

#SET (y, zzz.GetDateYYYYMMDD()[1..4])

%'y'%;
(string10)zzz.GetDateYYYYMMDD();
