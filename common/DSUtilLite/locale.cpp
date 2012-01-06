/*
 *      Copyright (C) 2010-2012 Hendrik Leppkes
 *      http://www.1f0.de
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 *  Initial design and concept by Gabest and the MPC-HC Team, copyright under GPLv2
 */

#include "stdafx.h"
#include "DShowUtil.h"

static struct {LPCSTR name, iso6392, iso6391, iso6392_2; LCID lcid;} s_isolangs[] =	// TODO : fill LCID !!!
{
  {"Abkhazian", "abk", "ab"},
  {"Achinese", "ace", NULL},
  {"Acoli", "ach", NULL},
  {"Adangme", "ada", NULL},
  {"Afar", "aar", "aa"},
  {"Afrihili", "afh", NULL},
  {"Afrikaans", "afr", "af", NULL, MAKELCID( MAKELANGID(LANG_AFRIKAANS, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Afro-Asiatic (Other)", "afa", NULL},
  {"Akan", "aka", "ak"},
  {"Akkadian", "akk", NULL},
  {"Albanian", "sqi", "sq", "alb", MAKELCID( MAKELANGID(LANG_ALBANIAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Aleut", "ale", NULL},
  {"Algonquian languages", "alg", NULL},
  {"Altaic (Other)", "tut", NULL},
  {"Amharic", "amh", "am"},
  {"Apache languages", "apa", NULL},
  {"Arabic", "ara", "ar", NULL, MAKELCID( MAKELANGID(LANG_ARABIC, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Aragonese", "arg", "an"},
  {"Aramaic", "arc", NULL},
  {"Arapaho", "arp", NULL},
  {"Araucanian", "arn", NULL},
  {"Arawak", "arw", NULL},
  {"Armenian", "arm", "hy",	"hye", MAKELCID( MAKELANGID(LANG_ARMENIAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Artificial (Other)", "art", NULL},
  {"Assamese", "asm", "as", NULL, MAKELCID( MAKELANGID(LANG_ASSAMESE, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Asturian; Bable", "ast", NULL},
  {"Athapascan languages", "ath", NULL},
  {"Australian languages", "aus", NULL},
  {"Austronesian (Other)", "map", NULL},
  {"Avaric", "ava", "av"},
  {"Avestan", "ave", "ae"},
  {"Awadhi", "awa", NULL},
  {"Aymara", "aym", "ay"},
  {"Azerbaijani", "aze", "az", NULL, MAKELCID( MAKELANGID(LANG_AZERI, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Bable; Asturian", "ast", NULL},
  {"Balinese", "ban", NULL},
  {"Baltic (Other)", "bat", NULL},
  {"Baluchi", "bal", NULL},
  {"Bambara", "bam", "bm"},
  {"Bamileke languages", "bai", NULL},
  {"Banda", "bad", NULL},
  {"Bantu (Other)", "bnt", NULL},
  {"Basa", "bas", NULL},
  {"Bashkir", "bak", "ba", NULL, MAKELCID( MAKELANGID(LANG_BASHKIR, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Basque", "baq", "eu", "eus", MAKELCID( MAKELANGID(LANG_BASQUE, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Batak (Indonesia)", "btk", NULL},
  {"Beja", "bej", NULL},
  {"Belarusian", "bel", "be", NULL, MAKELCID( MAKELANGID(LANG_BELARUSIAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Bemba", "bem", NULL},
  {"Bengali", "ben", "bn", NULL, MAKELCID( MAKELANGID(LANG_BENGALI, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Berber (Other)", "ber", NULL},
  {"Bhojpuri", "bho", NULL},
  {"Bihari", "bih", "bh"},
  {"Bikol", "bik", NULL},
  {"Bini", "bin", NULL},
  {"Bislama", "bis", "bi"},
  {"Bokmål, Norwegian; Norwegian Bokmål", "nob", "nb"},
  {"Bosnian", "bos", "bs"},
  {"Braj", "bra", NULL},
  {"Breton", "bre", "br", NULL, MAKELCID( MAKELANGID(LANG_BRETON, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Buginese", "bug", NULL},
  {"Bulgarian", "bul", "bg", NULL, MAKELCID( MAKELANGID(LANG_BULGARIAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Buriat", "bua", NULL},
  {"Burmese", "bur", "my", "mya"},
  {"Caddo", "cad", NULL},
  {"Carib", "car", NULL},
  {"Spanish", "spa", "es", "esp", MAKELCID( MAKELANGID(LANG_SPANISH, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Catalan", "cat", "ca", NULL, MAKELCID( MAKELANGID(LANG_CATALAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Caucasian (Other)", "cau", NULL},
  {"Cebuano", "ceb", NULL},
  {"Celtic (Other)", "cel", NULL},
  {"Central American Indian (Other)", "cai", NULL},
  {"Chagatai", "chg", NULL},
  {"Chamic languages", "cmc", NULL},
  {"Chamorro", "cha", "ch"},
  {"Chechen", "che", "ce"},
  {"Cherokee", "chr", NULL},
  {"Chewa; Chichewa; Nyanja", "nya", "ny"},
  {"Cheyenne", "chy", NULL},
  {"Chibcha", "chb", NULL},
  {"Chinese", "chi", "zh", "zho", MAKELCID( MAKELANGID(LANG_CHINESE, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Chinook jargon", "chn", NULL},
  {"Chipewyan", "chp", NULL},
  {"Choctaw", "cho", NULL},
  {"Chuang; Zhuang", "zha", "za"},
  {"Church Slavic; Old Church Slavonic", "chu", "cu"},
  {"Old Church Slavonic; Old Slavonic; ", "chu", "cu"},
  {"Chuukese", "chk", NULL},
  {"Chuvash", "chv", "cv"},
  {"Coptic", "cop", NULL},
  {"Cornish", "cor", "kw"},
  {"Corsican", "cos", "co", NULL, MAKELCID( MAKELANGID(LANG_CORSICAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Cree", "cre", "cr"},
  {"Creek", "mus", NULL},
  {"Creoles and pidgins (Other)", "crp", NULL},
  {"Creoles and pidgins,", "cpe", NULL},
  //   {"English-based (Other)", NULL, NULL},
  {"Creoles and pidgins,", "cpf", NULL},
  //   {"French-based (Other)", NULL, NULL},
  {"Creoles and pidgins,", "cpp", NULL},
  //   {"Portuguese-based (Other)", NULL, NULL},
  {"Croatian", "scr", "hr", "hrv", MAKELCID( MAKELANGID(LANG_CROATIAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Cushitic (Other)", "cus", NULL},
  {"Czech", "cze", "cs", "ces", MAKELCID( MAKELANGID(LANG_CZECH, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Dakota", "dak", NULL},
  {"Danish", "dan", "da", NULL, MAKELCID( MAKELANGID(LANG_DANISH, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Dargwa", "dar", NULL},
  {"Dayak", "day", NULL},
  {"Delaware", "del", NULL},
  {"Dinka", "din", NULL},
  {"Divehi", "div", "dv", NULL, MAKELCID( MAKELANGID(LANG_DIVEHI, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Dogri", "doi", NULL},
  {"Dogrib", "dgr", NULL},
  {"Dravidian (Other)", "dra", NULL},
  {"Duala", "dua", NULL},
  {"Dutch", "dut", "nl", "nld", MAKELCID( MAKELANGID(LANG_DUTCH, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Dutch, Middle (ca. 1050-1350)", "dum", NULL},
  {"Dyula", "dyu", NULL},
  {"Dzongkha", "dzo", "dz"},
  {"Efik", "efi", NULL},
  {"Egyptian (Ancient)", "egy", NULL},
  {"Ekajuk", "eka", NULL},
  {"Elamite", "elx", NULL},
  {"English", "eng", "en", NULL, MAKELCID( MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"English, Middle (1100-1500)", "enm", NULL},
  {"English, Old (ca.450-1100)", "ang", NULL},
  {"Esperanto", "epo", "eo"},
  {"Estonian", "est", "et", NULL, MAKELCID( MAKELANGID(LANG_ESTONIAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Ewe", "ewe", "ee"},
  {"Ewondo", "ewo", NULL},
  {"Fang", "fan", NULL},
  {"Fanti", "fat", NULL},
  {"Faroese", "fao", "fo", NULL, MAKELCID( MAKELANGID(LANG_FAEROESE, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Fijian", "fij", "fj"},
  {"Finnish", "fin", "fi", NULL, MAKELCID( MAKELANGID(LANG_FINNISH, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Finno-Ugrian (Other)", "fiu", NULL},
  {"Flemish; Dutch", "dut", "nl"},
  {"Flemish; Dutch", "nld", "nl"},
  {"Fon", "fon", NULL},
  {"French", "fre", "fr", "fra", MAKELCID( MAKELANGID(LANG_FRENCH, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"French, Middle (ca.1400-1600)", "frm", NULL},
  {"French, Old (842-ca.1400)", "fro", NULL},
  {"Frisian", "fry", "fy", NULL, MAKELCID( MAKELANGID(LANG_FRISIAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Friulian", "fur", NULL},
  {"Fulah", "ful", "ff"},
  {"Ga", "gaa", NULL},
  {"Gaelic; Scottish Gaelic", "gla", "gd", NULL, MAKELCID( MAKELANGID(LANG_GALICIAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Gallegan", "glg", "gl"},
  {"Ganda", "lug", "lg"},
  {"Gayo", "gay", NULL},
  {"Gbaya", "gba", NULL},
  {"Geez", "gez", NULL},
  {"Georgian", "geo", "ka", "kat", MAKELCID( MAKELANGID(LANG_GEORGIAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"German", "ger", "de", "deu", MAKELCID( MAKELANGID(LANG_GERMAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"German, Low; Saxon, Low; Low German; Low Saxon", "nds", NULL},
  {"German, Middle High (ca.1050-1500)", "gmh", NULL},
  {"German, Old High (ca.750-1050)", "goh", NULL},
  {"Germanic (Other)", "gem", NULL},
  {"Gikuyu; Kikuyu", "kik", "ki"},
  {"Gilbertese", "gil", NULL},
  {"Gondi", "gon", NULL},
  {"Gorontalo", "gor", NULL},
  {"Gothic", "got", NULL},
  {"Grebo", "grb", NULL},
  {"Ancient Greek", "grc", NULL},
  {"Greek", "gre", "el", "ell", MAKELCID( MAKELANGID(LANG_GREEK, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Greenlandic; Kalaallisut", "kal", "kl", NULL, MAKELCID( MAKELANGID(LANG_GREENLANDIC, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Guarani", "grn", "gn"},
  {"Gujarati", "guj", "gu", NULL, MAKELCID( MAKELANGID(LANG_GUJARATI, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Gwich´in", "gwi", NULL},
  {"Haida", "hai", NULL},
  {"Hausa", "hau", "ha", NULL, MAKELCID( MAKELANGID(LANG_HAUSA, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Hawaiian", "haw", NULL},
  {"Hebrew", "heb", "he", NULL, MAKELCID( MAKELANGID(LANG_HEBREW, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Herero", "her", "hz"},
  {"Hiligaynon", "hil", NULL},
  {"Himachali", "him", NULL},
  {"Hindi", "hin", "hi", NULL, MAKELCID( MAKELANGID(LANG_HINDI, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Hiri Motu", "hmo", "ho"},
  {"Hittite", "hit", NULL},
  {"Hmong", "hmn", NULL},
  {"Hungarian", "hun", "hu", NULL, MAKELCID( MAKELANGID(LANG_HUNGARIAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Hupa", "hup", NULL},
  {"Iban", "iba", NULL},
  {"Icelandic", "ice", "is", "isl", MAKELCID( MAKELANGID(LANG_ICELANDIC, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Ido", "ido", "io"},
  {"Igbo", "ibo", "ig", NULL, MAKELCID( MAKELANGID(LANG_IGBO, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Ijo", "ijo", NULL},
  {"Iloko", "ilo", NULL},
  {"Inari Sami", "smn", NULL},
  {"Indic (Other)", "inc", NULL},
  {"Indo-European (Other)", "ine", NULL},
  {"Indonesian", "ind", "id", NULL, MAKELCID( MAKELANGID(LANG_INDONESIAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Ingush", "inh", NULL},
  {"Interlingua (International", "ina", "ia"},
  //   {"Auxiliary Language Association)", NULL, NULL},
  {"Interlingue", "ile", "ie"},
  {"Inuktitut", "iku", "iu", NULL, MAKELCID( MAKELANGID(LANG_INUKTITUT, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Inupiaq", "ipk", "ik"},
  {"Iranian (Other)", "ira", NULL},
  {"Irish", "gle", "ga", NULL, MAKELCID( MAKELANGID(LANG_IRISH, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Irish, Middle (900-1200)", "mga", NULL, NULL, MAKELCID( MAKELANGID(LANG_IRISH, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Irish, Old (to 900)", "sga", NULL, NULL, MAKELCID( MAKELANGID(LANG_IRISH, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Iroquoian languages", "iro", NULL},
  {"Italian", "ita", "it", NULL, MAKELCID( MAKELANGID(LANG_ITALIAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Japanese", "jpn", "ja", NULL, MAKELCID( MAKELANGID(LANG_JAPANESE, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Javanese", "jav", "jv"},
  {"Judeo-Arabic", "jrb", NULL},
  {"Judeo-Persian", "jpr", NULL},
  {"Kabardian", "kbd", NULL},
  {"Kabyle", "kab", NULL},
  {"Kachin", "kac", NULL},
  {"Kalaallisut; Greenlandic", "kal", "kl"},
  {"Kamba", "kam", NULL},
  {"Kannada", "kan", "kn", NULL, MAKELCID( MAKELANGID(LANG_KANNADA, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Kanuri", "kau", "kr"},
  {"Kara-Kalpak", "kaa", NULL},
  {"Karen", "kar", NULL},
  {"Kashmiri", "kas", "ks", NULL, MAKELCID( MAKELANGID(LANG_KASHMIRI, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Kawi", "kaw", NULL},
  {"Kazakh", "kaz", "kk", NULL, MAKELCID( MAKELANGID(LANG_KAZAK, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Khasi", "kha", NULL},
  {"Khmer", "khm", "km", NULL, MAKELCID( MAKELANGID(LANG_KHMER, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Khoisan (Other)", "khi", NULL},
  {"Khotanese", "kho", NULL},
  {"Kikuyu; Gikuyu", "kik", "ki"},
  {"Kimbundu", "kmb", NULL},
  {"Kinyarwanda", "kin", "rw", NULL, MAKELCID( MAKELANGID(LANG_KINYARWANDA, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Kirghiz", "kir", "ky"},
  {"Komi", "kom", "kv"},
  {"Kongo", "kon", "kg"},
  {"Konkani", "kok", NULL, NULL, MAKELCID( MAKELANGID(LANG_KONKANI, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Korean", "kor", "ko", NULL, MAKELCID( MAKELANGID(LANG_KOREAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Kosraean", "kos", NULL},
  {"Kpelle", "kpe", NULL},
  {"Kru", "kro", NULL},
  {"Kuanyama; Kwanyama", "kua", "kj"},
  {"Kumyk", "kum", NULL},
  {"Kurdish", "kur", "ku"},
  {"Kurukh", "kru", NULL},
  {"Kutenai", "kut", NULL},
  {"Kwanyama, Kuanyama", "kua", "kj"},
  {"Ladino", "lad", NULL},
  {"Lahnda", "lah", NULL},
  {"Lamba", "lam", NULL},
  {"Lao", "lao", "lo", NULL, MAKELCID( MAKELANGID(LANG_LAO, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Latin", "lat", "la"},
  {"Latvian", "lav", "lv", NULL, MAKELCID( MAKELANGID(LANG_LATVIAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Letzeburgesch; Luxembourgish", "ltz", "lb"},
  {"Lezghian", "lez", NULL},
  {"Limburgan; Limburger; Limburgish", "lim", "li"},
  {"Lingala", "lin", "ln"},
  {"Lithuanian", "lit", "lt", NULL, MAKELCID( MAKELANGID(LANG_LITHUANIAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Lozi", "loz", NULL},
  {"Luba-Katanga", "lub", "lu"},
  {"Luba-Lulua", "lua", NULL},
  {"Luiseno", "lui", NULL},
  {"Lule Sami", "smj", NULL},
  {"Lunda", "lun", NULL},
  {"Luo (Kenya and Tanzania)", "luo", NULL},
  {"Lushai", "lus", NULL},
  {"Luxembourgish; Letzeburgesch", "ltz", "lb", NULL, MAKELCID( MAKELANGID(LANG_LUXEMBOURGISH, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Macedonian", "mac", "mk", "mkd", MAKELCID( MAKELANGID(LANG_MACEDONIAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Madurese", "mad", NULL},
  {"Magahi", "mag", NULL},
  {"Maithili", "mai", NULL},
  {"Makasar", "mak", NULL},
  {"Malagasy", "mlg", "mg"},
  {"Malay", "may", "ms", "msa", MAKELCID( MAKELANGID(LANG_MALAY, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Malayalam", "mal", "ml", NULL, MAKELCID( MAKELANGID(LANG_MALAYALAM, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Maltese", "mlt", "mt", NULL, MAKELCID( MAKELANGID(LANG_MALTESE, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Manchu", "mnc", NULL},
  {"Mandar", "mdr", NULL},
  {"Mandingo", "man", NULL},
  {"Manipuri", "mni", NULL, NULL, MAKELCID( MAKELANGID(LANG_MANIPURI, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Manobo languages", "mno", NULL},
  {"Manx", "glv", "gv"},
  {"Maori", "mao", "mi", "mri", MAKELCID( MAKELANGID(LANG_MAORI, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Marathi", "mar", "mr", NULL, MAKELCID( MAKELANGID(LANG_MARATHI, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Mari", "chm", NULL},
  {"Marshallese", "mah", "mh"},
  {"Marwari", "mwr", NULL},
  {"Masai", "mas", NULL},
  {"Mayan languages", "myn", NULL},
  {"Mende", "men", NULL},
  {"Micmac", "mic", NULL},
  {"Minangkabau", "min", NULL},
  {"Miscellaneous languages", "mis", NULL},
  {"Mohawk", "moh", NULL, NULL, MAKELCID( MAKELANGID(LANG_MOHAWK, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Moldavian", "mol", "mo"},
  {"Mon-Khmer (Other)", "mkh", NULL},
  {"Mongo", "lol", NULL},
  {"Mongolian", "mon", "mn", NULL, MAKELCID( MAKELANGID(LANG_MONGOLIAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Mossi", "mos", NULL},
  {"Multiple languages", "mul", NULL},
  {"Munda languages", "mun", NULL},
  {"Nahuatl", "nah", NULL},
  {"Nauru", "nau", "na"},
  {"Navaho, Navajo", "nav", "nv"},
  {"Ndebele, North", "nde", "nd"},
  {"Ndebele, South", "nbl", "nr"},
  {"Ndonga", "ndo", "ng"},
  {"Neapolitan", "nap", NULL},
  {"Nepali", "nep", "ne", NULL, MAKELCID( MAKELANGID(LANG_NEPALI, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Newari", "new", NULL},
  {"Nias", "nia", NULL},
  {"Niger-Kordofanian (Other)", "nic", NULL},
  {"Nilo-Saharan (Other)", "ssa", NULL},
  {"Niuean", "niu", NULL},
  {"Norse, Old", "non", NULL},
  {"North American Indian (Other)", "nai", NULL},
  {"Northern Sami", "sme", "se"},
  {"North Ndebele", "nde", "nd"},
  {"Norwegian", "nor", "no", NULL, MAKELCID( MAKELANGID(LANG_NORWEGIAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Norwegian Bokmål; Bokmål, Norwegian", "nob", "nb", NULL, MAKELCID( MAKELANGID(LANG_NORWEGIAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Norwegian Nynorsk; Nynorsk, Norwegian", "nno", "nn", NULL, MAKELCID( MAKELANGID(LANG_NORWEGIAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Nubian languages", "nub", NULL},
  {"Nyamwezi", "nym", NULL},
  {"Nyanja; Chichewa; Chewa", "nya", "ny"},
  {"Nyankole", "nyn", NULL},
  {"Nynorsk, Norwegian; Norwegian Nynorsk", "nno", "nn"},
  {"Nyoro", "nyo", NULL},
  {"Nzima", "nzi", NULL},
  {"Occitan (post 1500},; Provençal", "oci", "oc", NULL, MAKELCID( MAKELANGID(LANG_OCCITAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Ojibwa", "oji", "oj"},
  {"Old Bulgarian; Old Slavonic; Church Slavonic;", "chu", "cu"},
  {"Oriya", "ori", "or"},
  {"Oromo", "orm", "om"},
  {"Osage", "osa", NULL},
  {"Ossetian; Ossetic", "oss", "os"},
  {"Ossetic; Ossetian", "oss", "os"},
  {"Otomian languages", "oto", NULL},
  {"Pahlavi", "pal", NULL},
  {"Palauan", "pau", NULL},
  {"Pali", "pli", "pi"},
  {"Pampanga", "pam", NULL},
  {"Pangasinan", "pag", NULL},
  {"Panjabi", "pan", "pa"},
  {"Papiamento", "pap", NULL},
  {"Papuan (Other)", "paa", NULL},
  {"Persian", "per", "fa", "fas", MAKELCID( MAKELANGID(LANG_PERSIAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Persian, Old (ca.600-400 B.C.)", "peo", NULL},
  {"Philippine (Other)", "phi", NULL},
  {"Phoenician", "phn", NULL},
  {"Pohnpeian", "pon", NULL},
  {"Polish", "pol", "pl", NULL, MAKELCID( MAKELANGID(LANG_POLISH, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Portuguese", "por", "pt", NULL, MAKELCID( MAKELANGID(LANG_PORTUGUESE, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Prakrit languages", "pra", NULL},
  {"Provençal; Occitan (post 1500)", "oci", "oc"},
  {"Provençal, Old (to 1500)", "pro", NULL},
  {"Pushto", "pus", "ps"},
  {"Quechua", "que", "qu", NULL, MAKELCID( MAKELANGID(LANG_QUECHUA, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Raeto-Romance", "roh", "rm"},
  {"Rajasthani", "raj", NULL},
  {"Rapanui", "rap", NULL},
  {"Rarotongan", "rar", NULL},
  {"Reserved for local use", "qaa-qtz", NULL},
  {"Romance (Other)", "roa", NULL},
  {"Romanian", "rum", "ro", "ron", MAKELCID( MAKELANGID(LANG_ROMANIAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Romany", "rom", NULL},
  {"Rundi", "run", "rn"},
  {"Russian", "rus", "ru", NULL, MAKELCID( MAKELANGID(LANG_RUSSIAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Salishan languages", "sal", NULL},
  {"Samaritan Aramaic", "sam", NULL},
  {"Sami languages (Other)", "smi", NULL},
  {"Samoan", "smo", "sm"},
  {"Sandawe", "sad", NULL},
  {"Sango", "sag", "sg"},
  {"Sanskrit", "san", "sa", NULL, MAKELCID( MAKELANGID(LANG_SANSKRIT, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Santali", "sat", NULL},
  {"Sardinian", "srd", "sc"},
  {"Sasak", "sas", NULL},
  {"Scots", "sco", NULL},
  {"Scottish Gaelic; Gaelic", "gla", "gd"},
  {"Selkup", "sel", NULL},
  {"Semitic (Other)", "sem", NULL},
  {"Serbian", "srp", "sr", "scc", MAKELCID( MAKELANGID(LANG_SERBIAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Serer", "srr", NULL},
  {"Shan", "shn", NULL},
  {"Shona", "sna", "sn"},
  {"Sichuan Yi", "iii", "ii"},
  {"Sidamo", "sid", NULL},
  {"Sign languages", "sgn", NULL},
  {"Siksika", "bla", NULL},
  {"Sindhi", "snd", "sd", NULL, MAKELCID( MAKELANGID(LANG_SINDHI, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Sinhalese", "sin", "si", NULL, MAKELCID( MAKELANGID(LANG_SINHALESE, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Sino-Tibetan (Other)", "sit", NULL},
  {"Siouan languages", "sio", NULL},
  {"Skolt Sami", "sms", NULL},
  {"Slave (Athapascan)", "den", NULL},
  {"Slavic (Other)", "sla", NULL},
  {"Slovak", "slo", "sk", "slk", MAKELCID( MAKELANGID(LANG_SLOVAK, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Slovenian", "slv", "sl", NULL, MAKELCID( MAKELANGID(LANG_SLOVENIAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Sogdian", "sog", NULL},
  {"Somali", "som", "so"},
  {"Songhai", "son", NULL},
  {"Soninke", "snk", NULL},
  {"Sorbian languages", "wen", NULL},
  {"Sotho, Northern", "nso", NULL, NULL, MAKELCID( MAKELANGID(LANG_SOTHO, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Sotho, Southern", "sot", "st", NULL, MAKELCID( MAKELANGID(LANG_SOTHO, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"South American Indian (Other)", "sai", NULL},
  {"Southern Sami", "sma", NULL},
  {"South Ndebele", "nbl", "nr"},
  {"Sukuma", "suk", NULL},
  {"Sumerian", "sux", NULL},
  {"Sundanese", "sun", "su"},
  {"Susu", "sus", NULL},
  {"Swahili", "swa", "sw", NULL, MAKELCID( MAKELANGID(LANG_SWAHILI, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Swati", "ssw", "ss"},
  {"Swedish", "swe", "sv", NULL, MAKELCID( MAKELANGID(LANG_SWEDISH, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Syriac", "syr", NULL, NULL, MAKELCID( MAKELANGID(LANG_SYRIAC, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Tagalog", "tgl", "tl"},
  {"Tahitian", "tah", "ty"},
  {"Tai (Other)", "tai", NULL},
  {"Tajik", "tgk", "tg", NULL, MAKELCID( MAKELANGID(LANG_TAJIK, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Tamashek", "tmh", NULL},
  {"Tamil", "tam", "ta", NULL, MAKELCID( MAKELANGID(LANG_TAMIL, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Tatar", "tat", "tt", NULL, MAKELCID( MAKELANGID(LANG_TATAR, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Telugu", "tel", "te",  NULL, MAKELCID( MAKELANGID(LANG_TELUGU, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Tereno", "ter", NULL},
  {"Tetum", "tet", NULL},
  {"Thai", "tha", "th", NULL, MAKELCID( MAKELANGID(LANG_THAI, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Tibetan", "tib", "bo", "bod", MAKELCID( MAKELANGID(LANG_TIBETAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Tigre", "tig", NULL},
  {"Tigrinya", "tir", "ti", NULL, MAKELCID( MAKELANGID(LANG_TIGRIGNA, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Timne", "tem", NULL},
  {"Tiv", "tiv", NULL},
  {"Tlingit", "tli", NULL},
  {"Tok Pisin", "tpi", NULL},
  {"Tokelau", "tkl", NULL},
  {"Tonga (Nyasa)", "tog", NULL},
  {"Tonga (Tonga Islands)", "ton", "to"},
  {"Tsimshian", "tsi", NULL},
  {"Tsonga", "tso", "ts"},
  {"Tswana", "tsn", "tn", NULL, MAKELCID( MAKELANGID(LANG_TSWANA, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Tumbuka", "tum", NULL},
  {"Tupi languages", "tup", NULL},
  {"Turkish", "tur", "tr", NULL, MAKELCID( MAKELANGID(LANG_TURKISH, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Turkish, Ottoman (1500-1928)", "ota", NULL,	NULL, MAKELCID( MAKELANGID(LANG_TURKISH, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Turkmen", "tuk", "tk", NULL, MAKELCID( MAKELANGID(LANG_TURKMEN, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Tuvalu", "tvl", NULL},
  {"Tuvinian", "tyv", NULL},
  {"Twi", "twi", "tw"},
  {"Ugaritic", "uga", NULL},
  {"Uighur", "uig", "ug", NULL, MAKELCID( MAKELANGID(LANG_UIGHUR, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Ukrainian", "ukr", "uk", NULL, MAKELCID( MAKELANGID(LANG_UKRAINIAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Umbundu", "umb", NULL},
  {"Undetermined", "und", NULL},
  {"Urdu", "urd", "ur", NULL, MAKELCID( MAKELANGID(LANG_URDU, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Uzbek", "uzb", "uz", NULL, MAKELCID( MAKELANGID(LANG_UZBEK, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Vai", "vai", NULL},
  {"Venda", "ven", "ve"},
  {"Vietnamese", "vie", "vi", NULL, MAKELCID( MAKELANGID(LANG_VIETNAMESE, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Volapük", "vol", "vo"},
  {"Votic", "vot", NULL},
  {"Wakashan languages", "wak", NULL},
  {"Walamo", "wal", NULL},
  {"Walloon", "wln", "wa"},
  {"Waray", "war", NULL},
  {"Washo", "was", NULL},
  {"Welsh", "wel", "cy", "cym", MAKELCID( MAKELANGID(LANG_WELSH, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Wolof", "wol", "wo", NULL, MAKELCID( MAKELANGID(LANG_WOLOF, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Xhosa", "xho", "xh", NULL, MAKELCID( MAKELANGID(LANG_XHOSA, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Yakut", "sah", NULL, NULL, MAKELCID( MAKELANGID(LANG_YAKUT, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Yao", "yao", NULL},
  {"Yapese", "yap", NULL},
  {"Yiddish", "yid", "yi"},
  {"Yoruba", "yor", "yo", NULL, MAKELCID( MAKELANGID(LANG_YORUBA, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Yupik languages", "ypk", NULL},
  {"Zande", "znd", NULL},
  {"Zapotec", "zap", NULL},
  {"Zenaga", "zen", NULL},
  {"Zhuang; Chuang", "zha", "za"},
  {"Zulu", "zul", "zu", NULL, MAKELCID( MAKELANGID(LANG_ZULU, SUBLANG_DEFAULT), SORT_DEFAULT)},
  {"Zuni", "zun", NULL},
  {"Classical Newari", "nwc", NULL},
  {"Klingon", "tlh", NULL},
  {"Blin", "byn", NULL},
  {"Lojban", "jbo", NULL},
  {"Lower Sorbian", "dsb", NULL},
  {"Upper Sorbian", "hsb", NULL},
  {"Kashubian", "csb", NULL},
  {"Crimean Turkish", "crh", NULL},
  {"Erzya", "myv", NULL},
  {"Moksha", "mdf", NULL},
  {"Karachay-Balkar", "krc", NULL},
  {"Adyghe", "ady", NULL},
  {"Udmurt", "udm", NULL},
  {"Dargwa", "dar", NULL},
  {"Ingush", "inh", NULL},
  {"Nogai", "nog", NULL},
  {"Haitian", "hat", "ht"},
  {"Kalmyk", "xal", NULL},
  {NULL, NULL, NULL},
  {"No subtitles", "---", NULL, NULL, (LCID)LCID_NOSUBTITLES},
};

std::string ISO6391ToLanguage(LPCSTR code)
{
  CHAR tmp[2+1];
  strncpy_s(tmp, code, 2);
  tmp[2] = 0;
  _strlwr_s(tmp);
  for(size_t i = 0, j = countof(s_isolangs); i < j; i++) {
    if(s_isolangs[i].iso6391 && !strcmp(s_isolangs[i].iso6391, tmp))
    {
      std::string ret = std::string(s_isolangs[i].name);
      size_t i = ret.find(';');
      if(i != std::string::npos) {
        ret = ret.substr(0, i);
      }
      return ret;
    }
  }
  return std::string();
}

std::string ISO6392ToLanguage(LPCSTR code)
{
  CHAR tmp[3+1];
  strncpy_s(tmp, code, 3);
  tmp[3] = 0;
  _strlwr_s(tmp);
  for(size_t i = 0, j = countof(s_isolangs); i < j; i++) {
    if((s_isolangs[i].iso6392 && !strcmp(s_isolangs[i].iso6392, tmp)) || (s_isolangs[i].iso6392_2 && !strcmp(s_isolangs[i].iso6392_2, tmp)))
    {
      std::string ret = std::string(s_isolangs[i].name);
      size_t i = ret.find(';');
      if(i != std::string::npos) {
        ret = ret.substr(0, i);
      }
      return ret;
    }
  }
  return std::string();
}

std::string ProbeLangForLanguage(LPCSTR code)
{
  if(strlen(code) == 3) {
    return ISO6392ToLanguage(code);
  } else if (strlen(code) >= 2) {
    return ISO6391ToLanguage(code);
  }
  return std::string();
}

static std::string ISO6392Check(LPCSTR lang)
{
  CHAR tmp[3+1];
  strncpy_s(tmp, lang, 3);
  tmp[3] = 0;
  _strlwr_s(tmp);
  for(size_t i = 0, j = countof(s_isolangs); i < j; i++) {
    if((s_isolangs[i].iso6392 && !strcmp(s_isolangs[i].iso6392, tmp)) || (s_isolangs[i].iso6392_2 && !strcmp(s_isolangs[i].iso6392_2, tmp)))
    {
      return std::string(s_isolangs[i].iso6392);
    }
  }
  return std::string(tmp);
}

static std::string LanguageToISO6392(LPCSTR code)
{
  for(size_t i = 0, j = countof(s_isolangs); i < j; i++) {
    if((s_isolangs[i].name && !_stricmp(s_isolangs[i].name, code)))
    {
      return std::string(s_isolangs[i].iso6392);
    }
  }
  return std::string(code);
}

std::string ProbeForISO6392(LPCSTR lang)
{
  if (strlen(lang) == 2) {
    return ISO6391To6392(lang);
  } else if (strlen(lang) == 3) {
    return ISO6392Check(lang);
  } else if (strlen(lang) > 3) {
    return LanguageToISO6392(lang);
  }
  return std::string(lang);
}

LCID ISO6391ToLcid(LPCSTR code)
{
  CHAR tmp[2+1];
  strncpy_s(tmp, code, 2);
  tmp[2] = 0;
  _strlwr_s(tmp);
  for(size_t i = 0, j = countof(s_isolangs); i < j; i++) {
    if(s_isolangs[i].iso6391 && !strcmp(s_isolangs[i].iso6391, tmp))
    {
      return s_isolangs[i].lcid;
    }
  }
  return 0;
}

LCID ISO6392ToLcid(LPCSTR code)
{
  CHAR tmp[3+1];
  strncpy_s(tmp, code, 3);
  tmp[3] = 0;
  _strlwr_s(tmp);
  for(size_t i = 0, j = countof(s_isolangs); i < j; i++) {
    if((s_isolangs[i].iso6392 && !strcmp(s_isolangs[i].iso6392, tmp)) || (s_isolangs[i].iso6392_2 && !strcmp(s_isolangs[i].iso6392_2, tmp)))
    {
      return s_isolangs[i].lcid;
    }
  }
  return 0;
}

std::string ISO6391To6392(LPCSTR code)
{
  CHAR tmp[2+1];
  strncpy_s(tmp, code, 2);
  tmp[2] = 0;
  _strlwr_s(tmp);
  for(size_t i = 0, j = countof(s_isolangs); i < j; i++) {
    if(s_isolangs[i].iso6391 && !strcmp(s_isolangs[i].iso6391, tmp)) {
      return s_isolangs[i].iso6392;
    }
  }
  return std::string(code);
}

std::string ISO6392To6391(LPCSTR code)
{
  CHAR tmp[3+1];
  strncpy_s(tmp, code, 3);
  tmp[3] = 0;
  _strlwr_s(tmp);
  for(size_t i = 0, j = countof(s_isolangs); i < j; i++) {
    if((s_isolangs[i].iso6392 && !strcmp(s_isolangs[i].iso6392, tmp)) || (s_isolangs[i].iso6392_2 && !strcmp(s_isolangs[i].iso6392_2, tmp))) {
      return s_isolangs[i].iso6391;
    }
  }
  return std::string();
}

LCID ProbeLangForLCID(LPCSTR code)
{
  if(strlen(code) == 3) {
    return ISO6392ToLcid(code);
  } else if (strlen(code) >= 2) {
    return ISO6391ToLcid(code);
  }
  return 0;
}
