/* Create man pages from doxygen XML output.
 *
 * Georg Sauthoff <mail@georg.so>, 2012
 *
 * License: GPLv3+
 *
 */


// XXX TODO
// enums/defines?
// split this file


#include <QtXml>
#include <QStack>
#include <QTextStream>
#include <QSet>
#include <QMap>
#include <QXmlSchema>
#include <QXmlSchemaValidator>
#include <QCoreApplication>

#include <iostream>
#include <iomanip>
#include <algorithm>
#include <stdexcept>

using namespace std;

ostream &operator<<(ostream &o, const QString &q)
{
  o << q.toUtf8().data();
  return o;
}

struct doxy2man {
  static const char name[];
  static const char ver[];
};

const char doxy2man::name[] = "doxy2man";
const char doxy2man::ver[] = "0.1";

enum Direction { DIR_NONE, DIR_IN, DIR_OUT };

/** Temp structure
 */
struct Parameter_Item {
  QString name;
  Direction dir;
  QString desc;

  Parameter_Item()
    : dir(DIR_NONE)
  {
  }
};

struct Parameter {
  QString type;
  QString name;
  QString compound_ref;
  QString brief_desc;
  QString desc;
  Direction dir;

  Parameter()
    : dir(DIR_NONE)
  {
  }

  Parameter &operator=(const Parameter_Item &p)
  {
    dir = p.dir;
    desc = p.desc;
    return *this;
  }
};

struct See_Also {
  QString ref_id;
  QString name;

  See_Also() {}

  See_Also(const QString &x)
    : ref_id(x)
  {
  }
  void set_name(const QString &s)
  {
    name = s.trimmed();
  }
  void set_name_last(const QString &s)
  {
    int i = s.lastIndexOf(' ');
    if (i < 0)
      return;
    name = s.mid(i+1);
  }
};

struct Function {
  QString name;
  QVector<Parameter> parameters;
  QString type;
  QVector<QString> authors;
  QVector<Parameter> ret_values;
  QString brief_desc;
  QString desc;
  QString return_desc;
  QString copyright;

  QVector<QString> ref_ids;

  QVector<See_Also> see_also;

  int index_of_parameter(const QString &name)
  {
    int i = 0;
    QVectorIterator<Parameter> itr(parameters);
    while (itr.hasNext()) {
      if (itr.next().name == name)
        return i;
      ++i;
    }
    return -1;
  }
  bool has_detailed_param_desc() const
  {
    foreach (const Parameter &p, parameters) {
      if (!p.desc.isEmpty())
        return true;
    }
    return false;
  }
  bool operator<(const Function &other) const
  {
    return name < other.name;
  }
};

struct Member {
  QString name;
  QString type;
  QString desc;
  QString brief_desc;
};

struct Struct {
  QString id;
  QString name;
  QString desc;
  QString brief_desc;
  QVector<Member> members;
};

struct Options {
  QString exec_name;
  bool enable_warnings;
  bool just_dump;
  bool enable_summary_page;
  bool enable_copyright;
  bool enable_follow_refs;
  bool enable_validate;
  bool enable_seealso_all;
  bool enable_sort;
  bool enable_structs;
  QDir output_dir;
  QString output_dir_path;
  QString man_section;
  QString short_pkg;
  QString pkg;
  QString include_prefix;

  QString filename;
  QStringList filenames;
  QString base_path;

  Options()
    : enable_warnings(true),
    just_dump(false),
    enable_summary_page(true),
    enable_copyright(true),
    enable_follow_refs(true),
    enable_validate(true),
    enable_seealso_all(true),
    enable_sort(true),
    enable_structs(true),
    output_dir_path("out"),
    man_section("3"),
    short_pkg("XXXpkg"),
    pkg("The XXX Manual")
  {
  }

  void help()
  {
    cout << "Generates man pages from doxygen XML output\n";
    cout << "\n";
    cout << "call: " << exec_name << " OPTIONS DOXYGEN_XML_HEADER_FILE\n\n"
      << "where\n\n"
      << "-h,     --help           this screen\n"
      "        --nowarn         suppress warnings\n"
      "        --nosummary      don't generate summare man page\n"
      "        --nocopyright    don't generate copyright section\n"
      "        --nofollow       don't parse referenced xml files\n"
      "        --novalidate     don't validate xml files against compound.xsd\n"
      "        --noseealsoall   don't add all functions under see also\n"
      "        --nosort         don't sort functions under see also\n"
      "        --nostructs      don't print structs in function man pages\n"
      "-d,     --dump           just dump some input\n"
      "-o DIR, --out DIR        output directory\n"
      "-s STR, --section STR    man page section\n"
      "        --short-pkg STR  short man page header/footer string, e.g. 'Linux'\n"
      "        --pkg STR        man page header/footer string, e.g. 'Linux Programmer's Manual'\n"
      "-i STR, --include STR    include path prefix\n"
      << "\n";
  }
  void check_input_filename()
  {
    if (filenames.isEmpty()) {
      throw runtime_error( "No XML input file specified");
    }
    if (filenames.size() > 1)
      throw runtime_error("More than one input file specified");
    filename = filenames.front();
    QFile file(filename);
    if (!file.exists()) {
      QString msg("Input file ");
      msg += filename;
      msg += " does not exist";
      throw runtime_error(msg.toUtf8().data());
    }
    QFileInfo info(filename);
    base_path = info.path();
  }
  void parse(const QStringList &list)
  {
    bool read_dir = false;
    bool read_sec = false;
    bool read_include_prefix = false;
    bool read_short_pkg = false;
    bool read_pkg = false;
    bool only_filenames = false;
    QStringListIterator i(list);
    if (i.hasNext()) {
      exec_name = i.next();
    }
    while (i.hasNext()) {
      QString q(i.next());
      if (only_filenames) {
        filenames << q;
      }
      else if (read_dir) {
        output_dir_path = q;
        read_dir = false;
      }
      else if (read_sec) {
        man_section = q;
        read_sec = false;
      }
      else if (read_short_pkg) {
        short_pkg = q;
        read_short_pkg = false;
      }
      else if (read_pkg) {
        pkg = q;
        read_pkg = false;
      }
      else if (read_include_prefix) {
        include_prefix = q;
        read_include_prefix = false;
      }
      else if (q == "--nowarn")
        enable_warnings = false;
      else if (q == "--nosummary")
        enable_summary_page = false;
      else if (q == "--nocopyright")
        enable_copyright = false;
      else if (q == "--nofollow")
        enable_follow_refs = false;
      else if (q == "--novalidate")
        enable_validate = false;
      else if (q == "--noseealsoall")
        enable_seealso_all = false;
      else if (q == "--nosort")
        enable_sort = false;
      else if (q == "--nostructs")
        enable_structs = false;
      else if (q == "-d" || q == "--dump")
        just_dump = true;
      else if (q == "-o" || q == "--out")
        read_dir = true;
      else if (q == "-s" || q == "--section")
        read_sec = true;
      else if (q == "--short-pkg")
        read_short_pkg = true;
      else if (q == "--pkg")
        read_pkg = true;
      else if (q == "-i" || q == "--include-prefix")
        read_include_prefix = true;
      else if (q == "--")
        only_filenames = true;
      else if (q == "-h" || q == "--help") {
        help();
        exit(0);
      }
      else if (q.startsWith('-')) {
        QString s("Unknown option: ");
        s += q;
        throw runtime_error(s.toUtf8().data());
      } else {
        filenames << q;
      }
    }
    check_input_filename();
  }

  void check_create_output_dir()
  {
    if (!output_dir.mkpath(output_dir_path)) {
      QString m("Could not create output dir: ");
      m += output_dir_path;
      throw runtime_error(m.toUtf8().data());
    }
    output_dir.setPath(output_dir_path);
  }
};

struct Header {
  QString name;
  QString module_name; // e.g. without extension
  QString brief_desc;
  QString desc;
  QString copyright;

  QVector<Function> functions;
  QVector<Function> functions_sorted;
  QVector<Struct> structs;

  QSet<QString> ref_ids;
  QMap<QString, size_t> ref_id_struct_map;

  const Struct &struct_by_id(const QString &id) const
  {
    if (!ref_id_struct_map.contains(id)) {
      QString msg("unknown reference: ");
      msg += id;
      throw range_error(msg.toUtf8().data());
    }
    size_t i = ref_id_struct_map[id];
    return structs[i];
  }

  void sort(const Options &o)
  {
    functions_sorted = functions;
    if (o.enable_sort)
      qSort(functions_sorted.begin(), functions_sorted.end());
  }
  void check(ostream &o)
  {
    if (brief_desc.isEmpty())
      o << "Header file " << name << " has no brief description\n";
    if (brief_desc.size() > 70)
      o << "Brief description of " << name << " is not very brief\n";
    foreach (const Function &f, functions) {
      if (f.brief_desc.isEmpty())
        o << "Function " << f.name << " has no brief description\n";
      if (brief_desc.size() > 70)
        o << "The brief description of function " << f.name << " is not very brief\n";
    }
  }

};


QString ref2file(const QString &ref_id, const Options &o)
{
  QString name(ref_id);
  name += ".xml";
  QString filename(o.base_path);
  filename += QDir::separator();
  filename += name;
  QFile file(filename);
  QString msg("referenced file " + filename + " does not exist");
  if (!file.exists()) {
    throw runtime_error(msg.toUtf8().data());
  }
  return filename;
}


void add_to_list(int argc, char **argv, QStringList &list)
{
  for (int i = 0; i <argc; ++i)
    list << argv[i];
}


void print_dump(ostream &o, const Header &h)
{

  o << "File: " << h.name << '\n';
  o << h.brief_desc << '\n';
  o << "Detailed: " << h.desc << '\n';

  foreach (const Function &f, h.functions) {
    o << f.type << ' ' << f.name << "\n"
      << "    (\n";
    QVectorIterator<Parameter> i(f.parameters);

    Parameter a;
    if (i.hasNext()) {
      a = i.next();
      o << "        " << a.type << ' ' << a.name;
    }
    if (!i.hasNext()) {
      if (!a.brief_desc.isEmpty())
        o << " // " << a.brief_desc;
    } else while (i.hasNext()) {
      o << ",";
      if (!a.brief_desc.isEmpty())
        o << " // " << a.brief_desc;
      o << '\n';
      a = i.next();
      o << "        " << a.type << ' ' << a.name;
    }

    o << "\n    )\n"
      << "    " << f.brief_desc << "\n\n"
      << "    " << f.desc  << '\n'
      << "    Author: " << (f.authors.empty() ? QString() : f.authors[0])  << '\n'
      ;
    o << "    Parameters:\n";
    foreach (const Parameter &p, f.parameters) {
      o << "      " << p.name << ' ' << p.brief_desc << " || " << p.desc << '\n';
    }
    o << "    Ret Values:\n";
    foreach (const Parameter &p, f.ret_values) {
      o << "      " << p.name << ' ' << " || " << p.desc << '\n';
    }
    o << '\n';
  }

  foreach (const Struct &st, h.structs) {
    o << "struct " << st.name << '\n';
  }
}

size_t get_type_width(const QVector<Function> &list)
{
  size_t w = 5;
  foreach (const Function &f, list) {
    if (f.type.size() > w)
      w = f.type.size();
  }
  return w+1;
}

size_t get_type_width(const QVector<Member> &list)
{
  size_t w = 8;
  foreach (const Member &f, list) {
    if (f.type.size() > w)
      w = f.type.size();
  }
  return w+1;
}

size_t get_type_width(const QVector<Parameter> &list)
{
  size_t w = 8;
  foreach (const Parameter &f, list) {
    QString a(f.type.trimmed());
    if (a.size() > w)
      w = a.size();
  }
  return w+1;
}

QString fill_right(const QString &s, size_t w)
{
  QString a = s.trimmed();
  if (a.size() >=  w)
    return a;
  QString wstr(w-a.size(), ' ');
  if (!a.isEmpty() && a[a.size()-1] == '*') {
    int i = a.size()-2;
    for (; i>0; --i)
      if (a[i] != '*')
        break;
    ++i;
    a.insert(i, wstr);
  } else
    a.append(wstr);
  return a;
}

QString first_line(const QString &s)
{
  QString a = s.trimmed();
  int i = a.indexOf('\n');
  if (i == -1)
    return a;
  return a.left(i);
}

QStringList extract_authors(const QVector<Function> &functions)
{
  QStringList list;
  foreach (const Function &f, functions) {
    foreach (const QString &a, f.authors) {
      list << a;
    }
  }
  // list.sort();
  // QStringList::iterator end = unique(list.begin(), list.end());
  list.removeDuplicates();
  return list;
}

size_t max_member_size(const QVector<Member> &parameters)
{
  size_t r = 0;
  foreach (const Member &p, parameters) {
    size_t x = p.name.size();
    if (x > r)
      r = x;
  }
  return r;
}

void print_brief(QTextStream &o, size_t w, const Member &p)
{
  if (p.brief_desc.isEmpty())
    return;
  size_t used = p.name.size();
  QString wstr(w > used ? (w-used) : 0, ' ');
  o << wstr << " // " << p.brief_desc;

}

QString remove_fullstop(const QString &x)
{
  QString s(x.trimmed());
  if (s.endsWith('.'))
    return s.left(s.size()-1);
  return s;
}

void print_struct(QTextStream &o, const Struct &s)
{
    o << ".SS \""; // subsection
    o << remove_fullstop(s.brief_desc);
    o << "\"\n";
    o << ".PP\n"; // vert space, restore indent/margin
    o << ".sp\n"; // space
    QStringList paras = s.desc.split("\n", QString::SkipEmptyParts);
    foreach (const QString p, paras) {
      o << ".PP \n"; // line break, vert space, restore left margin/indent
      o << p << '\n';
    }
    o << ".sp\n";
    o << ".RS\n"; // reset left margin
    o << ".nf\n"; // no filling of output lines
    o << "\\fB\n"; // font to bold face
    o << "struct " << s.name << " {\n";
    size_t w = get_type_width(s.members);
    size_t name_size = max_member_size(s.members);
    foreach (const Member &m, s.members) {
      o << "  " << fill_right(m.type, w)  << "\\fI" << m.name << "\\fP;";
      print_brief(o, name_size, m);
      o << "\n";
    }
    o << "};\n";
    o << "\\fP\n"; // font back to previous face
    o << ".fi\n"; // fill output lines
    o << ".RE\n"; // move left margin back to the left
}

void print_man_summary(QTextStream &o, const Header &h, const Options &opts)
{
  o << ".\\\" File automatically generated by " << doxy2man::name << doxy2man::ver << '\n';
  o << ".\\\" Generation date: " << QDate::currentDate().toString() << '\n';

  o << ".TH " << h.module_name << ' ' << opts.man_section << ' '
    << QDate::currentDate().toString("yyyy-MM-dd") << " \"" << opts.short_pkg << "\" \""
    << opts.pkg << "\"\n";

  o << ".SH \"NAME\"\n"
    << h.name << " \\- " << first_line(h.brief_desc) << '\n';

  o << ".SH SYNOPSIS\n";
  o << ".nf\n"; // no filling of output lines
  o << ".B #include <" << opts.include_prefix << h.name << ">\n";
  o << ".fi\n"; // fill output lines

  o << ".SH DESCRIPTION\n";
  
  QStringList paras = h.desc.split("\n", QString::SkipEmptyParts);
  foreach (const QString p, paras) {
    o << ".PP \n"; // line break, vert space, restore left margin/indent
    o << p << '\n';
  }

  o << ".PP\n";
  // o << ".sp\n"; // one blank line
  // functions text ...
  o << ".sp\n";
  o << ".RS\n"; // move left margin to the right
  o << ".nf\n"; // no filling of output lines
  o << "\\fB\n"; // font to bold face
  size_t w = get_type_width(h.functions);
  foreach (const Function &f, h.functions) {
    o << fill_right(f.type, w) << f.name << "(";
    QVectorIterator<Parameter> i(f.parameters);
    if (i.hasNext())
      o << i.next().type;
    while (i.hasNext()) {
      o << ", ";
      o << i.next().type;
    }
    o << ");\n";
  }
  o << "\\fP\n"; // font back to previous face
  o << ".fi\n"; // fill output lines
  o << ".RE\n"; // move left margin back to the left

  foreach (const Struct &s, h.structs) {
    print_struct(o, s);
  }

  o << ".SH SEE ALSO\n";
  o << ".PP\n"; // 'paragraph'
  o << ".nh\n"; // disable hyphenation
  o << ".ad l\n"; // left justified
  QVectorIterator<Function> i(h.functions_sorted);
  if (i.hasNext())
    o << "\\fI" << i.next().name << "\\fP(" << opts.man_section << ")";
  while (i.hasNext()) {
    o << ", ";
    o << "\\fI" << i.next().name << "\\fP(" << opts.man_section << ")";
  }
  o << '\n';
  o << ".ad\n"; // justified default (?)
  o << ".hy\n"; // enable hyphenation

  QStringList authors = extract_authors(h.functions);
  if (!authors.isEmpty()) {
    o << ".SH AUTHORS\n";
    o << ".nf\n"; // no filling of output lines
    foreach (const QString &author, authors)
      o << author << '\n';
    o << ".fi\n"; // fill output lines
  }

  if (opts.enable_copyright && !h.copyright.isEmpty()) {
    o << ".SH COPYRIGHT\n"; // section heading
    o << ".PP\n"; // paragraph
    o << h.copyright << '\n';
  }

}

void open_for_writing(QFile &file, const QString &full_name)
{
  if (!file.open(QFile::WriteOnly)) {
    QString m("Opening stream for writing failed: ");
    m += full_name;
    m += " (";
    m += file.errorString();
    m += ")";
    throw runtime_error(m.toUtf8().data());
  }
}

void flush_stream(QTextStream &o, const QString &full_name)
{
  o.flush();
  if (o.status() != QTextStream::Ok) {
    QString m("Opening stream for writing failed: ");
    m += full_name;
    throw runtime_error(m.toUtf8().data());
  }
}

void print_man_summary_page(const Header &h, const Options &opts)
{
  if (!opts.enable_summary_page)
    return;

    QString page_name(h.name);
    page_name += '.';
    page_name += opts.man_section;
    QString full_name(opts.output_dir.path() + QDir::separator() + page_name);

    QFile file(full_name);
    open_for_writing(file, full_name);
    QTextStream o(&file);

    print_man_summary(o, h, opts);

    flush_stream(o, full_name);
    file.close();
}

size_t max_param_size(const QVector<Parameter> &parameters)
{
  size_t r = 0;
  foreach (const Parameter &p, parameters) {
    size_t x = p.name.size();
    if (x > r)
      r = x;
  }
  return r;
}

void print_brief(QTextStream &o, size_t w, const Parameter &p)
{
  if (p.brief_desc.isEmpty())
    return;
  size_t used = p.name.size();
  QString wstr(w > used ? (w-used) : 0, ' ');
  o << wstr << " // " << p.brief_desc;

}

void print_man_function(QTextStream &o, const Function &f, const Header &h,
    const Options &opts)
{
  o << ".\\\" File automatically generated by "
    << doxy2man::name << doxy2man::ver << '\n';
  o << ".\\\" Generation date: " << QDate::currentDate().toString() << '\n';

  o << ".TH " << f.name << ' ' << opts.man_section << ' '
    << QDate::currentDate().toString("yyyy-MM-dd") << " \""
    << opts.short_pkg << "\" \"" << opts.pkg << "\"\n";

  o << ".SH \"NAME\"\n"
    << f.name << " \\- " << first_line(f.brief_desc) << '\n';

  o << ".SH SYNOPSIS\n";
  o << ".nf\n"; // no filling of output lines
  o << ".B #include <" << opts.include_prefix << h.name << ">\n";
  o << ".sp\n"; // space line


  o << "\\fB" << f.type << ' ' << f.name << "\\fP(\n";
  size_t w = get_type_width(f.parameters);
  size_t param_size = max_param_size(f.parameters);
  QVectorIterator<Parameter> i(f.parameters);
  Parameter a;
  if (i.hasNext()) {
    a = i.next();
    // bold face, previous selected face
    o << "    \\fB" << fill_right(a.type, w) << "\\fP\\fI" << a.name << "\\fP";
  }
  if (!i.hasNext()) {
    print_brief(o, param_size, a);
  } else while (i.hasNext()) {
    o << ",";
    print_brief(o, param_size, a);
    o << '\n';
    a = i.next();
    o << "    \\fB" << fill_right(a.type, w) << "\\fP\\fI" << a.name << "\\fP";
  }
  o << "\n);\n";

  o << ".fi\n"; // fill output lines


  o << ".SH DESCRIPTION\n";
  QStringList paras = f.desc.split("\n", QString::SkipEmptyParts);
  foreach (const QString p, paras) {
    o << ".PP \n"; // line break, vert space, restore left margin/indent
    o << p << '\n';
  }

  if (f.has_detailed_param_desc()) {
    o << ".SH PARAMETERS\n";
    foreach (const Parameter &p, f.parameters) {
      o << ".TP\n"; // indented labeled paragraph, next line is label
      o << ".B "; // bold face
      o << p.name << '\n';
      if (p.desc.isEmpty())
        o << p.brief_desc;
      else
        o << p.desc;
      o << '\n';
    }
  }

  if (opts.enable_structs && !f.ref_ids.isEmpty()) {
    o << ".SH STRUCTURES\n";
    try {
    foreach (const QString &ref_id, f.ref_ids) {
      const Struct &s = h.struct_by_id(ref_id);
      print_struct(o, s);
    }
    } catch (const range_error &r) {
      cerr << "Warning: could not find referenced structure: " << r.what() << "(in "
        << f.name << ")\n";
    }
  }

  if (!f.return_desc.isEmpty() || !f.ret_values.isEmpty()) {
    o << ".SH RETURN VALUE\n";
    if (!f.return_desc.isEmpty()) {
      o << ".PP\n";
      o << f.return_desc << '\n';
    }
    foreach (const Parameter &p, f.ret_values) {
      o << ".TP\n"; // indented labeled paragraph, next line is label
      o << ".B "; // bold face
      o << p.name << '\n';
      o << p.desc << '\n';
    }
  }

  o << ".SH SEE ALSO\n";
  o << ".PP\n"; // 'paragraph'
  o << ".nh\n"; // disable hyphenation
  o << ".ad l\n"; // left justified
  o << "\\fI" << h.name << "\\fP(" << opts.man_section << ")";
  if (opts.enable_seealso_all) {
    foreach (const Function &i, h.functions_sorted) {
      o << ", ";
      o << "\\fI" << i.name << "\\fP(" << opts.man_section << ")";
    }
  }
  foreach (const See_Also &see, f.see_also) {
    o << ", " << "\\fI" << see.name << "\\fP";
  }
  o << '\n';
  o << ".ad\n"; // justified default (?)
  o << ".hy\n"; // enable hyphenation

  if (!f.authors.isEmpty()) {
    o << ".SH AUTHORS\n";
    o << ".nf\n"; // no filling of output lines
    foreach (const QString &author, f.authors)
      o << author << '\n';
    o << ".fi\n"; // fill output lines
  }

  if (opts.enable_copyright
      &&(!f.copyright.isEmpty() || !h.copyright.isEmpty())) {
    o << ".SH COPYRIGHT\n";
    o << ".PP\n"; // paragraph
    if (f.copyright.isEmpty())
      o << h.copyright << '\n';
    else
      o << f.copyright << '\n';
  }
}


void print_man(const Header &h, const Options &opts)
{
  print_man_summary_page(h, opts);

  foreach (const Function &f, h.functions) {
    QString page_name(f.name);
    page_name += '.';
    page_name += opts.man_section;
    QString full_name(opts.output_dir.path() + QDir::separator() + page_name);
    QFile file(full_name);
    open_for_writing(file, full_name);
    QTextStream o(&file);

    print_man_function(o, f, h, opts);

    flush_stream(o, full_name);
    file.close();
  }
}

enum Tag {
  TAG_IGNORE,
  TAG_SECTIONDEF_ENUM,
  TAG_SECTIONDEF_TYPEDEF,
  TAG_SECTIONDEF_FUNC,
  TAG_SECTIONDEF_DEFINE,
  TAG_MEMBERDEF_ENUM,
  TAG_MEMBERDEF_TYPDEF,
  TAG_MEMBERDEF_FUNC,
  TAG_MEMBERDEF_DEFINE,
  TAG_MEMBERDEF_VAR,
  TAG_NAME,
  TAG_ENUMVALUE,
  TAG_BRIEFDESC,
  TAG_DETAILDESC,
  TAG_TYPE,
  TAG_DEFINITION,
  TAG_ARGSTRING,
  TAG_PARAM,
  TAG_DECLNAME,
  TAG_COMPOUNDNAME,
  TAG_LINEBREAK,
  TAG_SIMPLESECT_AUTHOR,
  TAG_SIMPLESECT_RETURN,
  TAG_SIMPLESECT_COPYRIGHT,
  TAG_SIMPLESECT_SEE,
  TAG_PARAMETERLIST, // kind == param
  TAG_RETVALLIST, // fake parameterlist && kind == param
  TAG_PARAMETERNAME,
  TAG_PARAMETERDESC,
  TAG_PARAMETERITEM,
  TAG_REF, // refid xml file
  TAG_REF_MEMBER, // inline function ref (in briefdesc)
  TAG_COMPOUNDDEF_FILE,
  TAG_COMPOUNDDEF_STRUCT,
  TAG_ULINK, // mailto link ...
  TAG_PARA // paragraph
};

class Handler : public QXmlDefaultHandler
{
  public:
    Header &h;
  private:
    Tag tag;
  public:
    Handler(Header &header)
      : h(header), tag(TAG_IGNORE)
    {
    }
  private:
    Function f;
    Parameter p;
    Parameter_Item pi;
    Struct st;
    Member member;
  QString buffer;
  QStack<Tag> tag_stack;

  QString url;
  QString url_text;

  bool from_top(size_t i, Tag t)
  {
    if (i >= size_t(tag_stack.size()))
      return false;
    return tag_stack[tag_stack.size()-i-1] == t;
  }

  void parse_tag(const QString & qName, const QXmlAttributes & atts )
  {
    if (qName == "sectiondef" && atts.value("kind") == "func" ) {
      tag = TAG_SECTIONDEF_FUNC;
    } else if (qName == "memberdef") {
      if (atts.value("kind") == "function")
        tag = TAG_MEMBERDEF_FUNC;
      else if (atts.value("kind") == "variable")
        tag = TAG_MEMBERDEF_VAR;
      else
        tag = TAG_IGNORE;
    } else if (qName == "type") {
      tag = TAG_TYPE;
    } else if (qName == "definition") {
      tag = TAG_DEFINITION;
    } else if (qName == "name") {
      tag = TAG_NAME;
    } else if (qName == "param") {
      tag = TAG_PARAM;
    } else if (qName == "type") {
      tag = TAG_TYPE;
    } else if (qName == "declname") {
      tag = TAG_DECLNAME;
    } else if (qName == "briefdescription") {
      tag = TAG_BRIEFDESC;
    } else if (qName == "para") {
      tag = TAG_PARA;
    } else if (qName == "detaileddescription") {
      tag = TAG_DETAILDESC;
    } else if (qName == "parameterlist" && atts.value("kind") == "param") {
      tag  = TAG_PARAMETERLIST;
    } else if (qName == "parameterlist" && atts.value("kind") == "retval") {
      tag  = TAG_RETVALLIST;
    } else if  (qName == "parametername") {
      tag = TAG_PARAMETERNAME;
    } else if  (qName == "parameterdescription") {
      tag = TAG_PARAMETERDESC;
    } else if (qName == "simplesect") {
      if (atts.value("kind") == "author")
        tag = TAG_SIMPLESECT_AUTHOR;
      else if (atts.value("kind") == "return")
        tag = TAG_SIMPLESECT_RETURN;
      else if (atts.value("kind") == "copyright")
        tag = TAG_SIMPLESECT_COPYRIGHT;
      else if (atts.value("kind") == "see")
        tag = TAG_SIMPLESECT_SEE;
      else
        tag = TAG_IGNORE;
    } else if (qName == "parameteritem") {
      tag = TAG_PARAMETERITEM;
    } else if (qName == "compounddef") {
      if  (atts.value("kind") == "file")
        tag = TAG_COMPOUNDDEF_FILE;
      else if (atts.value("kind") == "struct")
        tag = TAG_COMPOUNDDEF_STRUCT;
      else
        tag = TAG_IGNORE;
    } else if (qName == "compoundname") {
      tag = TAG_COMPOUNDNAME;
    } else if (qName == "ulink") {
      tag = TAG_ULINK;
    } else if (qName == "ref") {
      if (atts.value("kindref") == "member") {
        tag = TAG_REF_MEMBER;
      } else
        tag = TAG_REF;
    } else {
      tag = TAG_IGNORE;
    }
  }

  bool startElement ( const QString & namespaceURI, const QString & localName, const QString & qName, const QXmlAttributes & atts )
  {

    //cout << qName.toUtf8().data() << '\n';
    parse_tag(qName, atts);
    if (tag != TAG_IGNORE && tag != TAG_ULINK && tag != TAG_REF && tag != TAG_REF_MEMBER)
      buffer.clear();
    tag_stack.push(tag);

    switch (tag) {
      case TAG_MEMBERDEF_FUNC:
        f = Function();
        break;
      case TAG_PARAM:
        p = Parameter();
        break;
      case TAG_REF:
        if (from_top(1, TAG_TYPE) && from_top(2, TAG_PARAM)
            && atts.value("kindref") == "compound") {
          p.compound_ref = atts.value("refid");
          f.ref_ids.push_back(p.compound_ref);
          h.ref_ids.insert(p.compound_ref);
        }
        break;
      case TAG_REF_MEMBER:
        if (from_top(1, TAG_PARA) && from_top(2, TAG_DETAILDESC)
            && from_top(3, TAG_MEMBERDEF_FUNC) ) {
          f.see_also.push_back(See_Also(atts.value("refid")));
        }
        break;
      case TAG_PARAMETERNAME:
        pi = Parameter_Item();
        pi.dir = DIR_NONE;
        if (atts.value("direction") == "in")
          pi.dir = DIR_IN;
        else if (atts.value("direction") == "out")
          pi.dir = DIR_OUT;
        break;
      case TAG_ULINK:
        url = atts.value("url");
        break;
      case TAG_MEMBERDEF_VAR:
        member = Member();
        break;
      case TAG_COMPOUNDDEF_STRUCT:
        st = Struct();
        st.id = atts.value("id");
        break;
    }

    return true;
  }

  bool  characters ( const QString & ch )
  {
    if (tag == TAG_ULINK) {
      url_text.append(ch);
      return true;
    }
    buffer.append(ch);
    return true;
  }

  bool  endElement ( const QString & namespaceURI, const QString & localName, const QString & qName )
  {
    //cout << buffer.toUtf8().data() << '\n';

    switch (tag) {
      case TAG_TYPE:
        if (from_top(1, TAG_MEMBERDEF_FUNC))
          f.type = buffer;
        else if (from_top(1, TAG_PARAM))
          p.type = buffer;
        else if (from_top(1, TAG_MEMBERDEF_VAR))
          member.type = buffer;
        break;
      case TAG_NAME:
        if (from_top(1, TAG_MEMBERDEF_FUNC))
          f.name = buffer;
        else if (from_top(1,TAG_MEMBERDEF_VAR))
          member.name = buffer;
        break;
      case TAG_MEMBERDEF_FUNC:
        h.functions.push_back(f);
        break;
      case TAG_BRIEFDESC:
        // usually a para inside is used
        // if (from_top(1, TAG_MEMBERDEF_FUNC))
        //  f.brief_desc += buffer;
        break;
      case TAG_PARA:
        if (from_top(1, TAG_BRIEFDESC)) {
          if (from_top(2, TAG_MEMBERDEF_FUNC))
            f.brief_desc = buffer;
          else if (from_top(2, TAG_PARAM))
            p.brief_desc = buffer;
          else if (from_top(2, TAG_COMPOUNDDEF_FILE))
            h.brief_desc = buffer;
          else if (from_top(2, TAG_MEMBERDEF_VAR))
            member.brief_desc = buffer;
          else if (from_top(2, TAG_COMPOUNDDEF_STRUCT))
            st.brief_desc = buffer;
        }
        else if (from_top(1, TAG_DETAILDESC)) {
          if (from_top(2, TAG_MEMBERDEF_FUNC)) {
            f.desc += buffer;
            f.desc += '\n';
          } else if (from_top(2, TAG_COMPOUNDDEF_FILE)) {
            h.desc += buffer;
            h.desc += '\n';
          } else if (from_top(2, TAG_MEMBERDEF_VAR)) {
            member.desc += buffer;
            member.desc += '\n';
          } else if (from_top(2, TAG_COMPOUNDDEF_STRUCT)) {
            st.desc += buffer;
            st.desc += '\n';
          }
        }
        else if (from_top(1, TAG_SIMPLESECT_AUTHOR)
              && from_top(3, TAG_DETAILDESC)
            && from_top(4, TAG_MEMBERDEF_FUNC)) {
          f.authors.push_back(buffer.trimmed());
          buffer.clear();
        }
        else if (from_top(1, TAG_PARAMETERDESC)) {
          pi.desc += buffer;
          pi.desc += '\n';
          buffer.clear();
        }
        else if (from_top(1, TAG_SIMPLESECT_RETURN)) {
          f.return_desc = buffer;
          buffer.clear();
        }
        else if (from_top(1, TAG_SIMPLESECT_COPYRIGHT)) {
          if (from_top(4, TAG_COMPOUNDDEF_FILE)) {
            h.copyright = buffer;
            buffer.clear();
          } else if (from_top(3, TAG_DETAILDESC) 
              && from_top(4, TAG_MEMBERDEF_FUNC)) {
            f.copyright = buffer;
            buffer.clear();
          }
        }
        else if (from_top(1, TAG_SIMPLESECT_SEE)
            && from_top(3, TAG_DETAILDESC)
            && from_top(4, TAG_MEMBERDEF_FUNC)) {
          f.see_also.push_back(See_Also());
          f.see_also.last().set_name(buffer);
          buffer.clear();
        }
        break;
      case TAG_DECLNAME:
        p.name = buffer;
        break;
      case TAG_PARAM:
        f.parameters.push_back(p);
        break;
      case TAG_PARAMETERNAME:
        pi.name = buffer;
        break;
      case TAG_PARAMETERITEM:
        if (from_top(1, TAG_PARAMETERLIST)) {
            int i = f.index_of_parameter(pi.name);
            if (i == -1) {
              qWarning() << "Can't find param name: " << pi.name;
            } else {
              f.parameters[i] = pi;
            }
        }
        if (from_top(1, TAG_RETVALLIST)) {
          Parameter a;
          a = pi;
          a.name = pi.name;
          f.ret_values.push_back(a);
        }
        break;
      case TAG_COMPOUNDNAME:
        if (from_top(1, TAG_COMPOUNDDEF_STRUCT)) {
          st.name = buffer;
        } else if (from_top(1, TAG_COMPOUNDDEF_FILE)) {
          h.name = buffer;
          h.module_name = h.name; // h.name.remove(".h").toUpper();
        }
        break;
      case TAG_ULINK:
        if (!url.isEmpty()) {
          if (url.startsWith("mailto:")) {
            buffer += "<";
            buffer += url.mid(7);
            buffer += ">";
          } else
            buffer += url;
        }
        url.clear();
        url_text.clear();
        break;
      case TAG_COMPOUNDDEF_STRUCT:
        h.ref_id_struct_map[st.id] = h.structs.size();
        h.structs.push_back(st);
        break;
      case TAG_MEMBERDEF_VAR:
        if (from_top(2, TAG_COMPOUNDDEF_STRUCT)) {
          st.members.push_back(member);
        }
        break;
      case TAG_REF_MEMBER:
        if (from_top(1, TAG_PARA) && from_top(2, TAG_DETAILDESC)
            && from_top(3, TAG_MEMBERDEF_FUNC) ) {
          f.see_also.last().set_name_last(buffer);
        }
        break;
    }

    tag_stack.pop();
    if (!tag_stack.empty())
      tag = tag_stack.top();
    return true;
  }
};

void validate(QFile &file, const Options &o)
{
  if (!o.enable_validate)
    return;

  QString xsd_filename(o.base_path + "/compound.xsd");
  QFile xsd_file(xsd_filename);
  if (!xsd_file.exists()) {
    QString msg("XSD ");
    msg += xsd_filename;
    msg += " does not exist";
    throw runtime_error(msg.toUtf8().data());
  }

  QUrl schemaUrl(QUrl::fromLocalFile(xsd_filename));
  QXmlSchema schema;
  schema.load(schemaUrl);

  if (schema.isValid()) {
    file.open(QIODevice::ReadOnly);
    QXmlSchemaValidator validator(schema);
    if (validator.validate(&file, QUrl::fromLocalFile(file.fileName()))) {
      file.seek(0);
    } else {
      QString msg("XML input ");
      msg += file.fileName();
      msg += " is invalid";
      throw runtime_error(msg.toUtf8().data());
    }
  } else {
    QString msg("XSD ");
    msg += xsd_filename;
    msg += " is invalid";
    throw runtime_error(msg.toUtf8().data());
  }
}

void parse_main_file(QXmlReader &reader, Handler &h, const Options &o)
{
  QFile file(o.filename);
  validate(file, o);
  QXmlInputSource source(&file);
  reader.setContentHandler(&h);
  reader.setErrorHandler(&h);
  bool pret = reader.parse(source);
  if (!pret) {
    QString msg("XML Parse error (");
    msg += o.filename;
    msg += ")";
    throw runtime_error(msg.toUtf8().data());
  }
}

void parse_refs(QXmlReader &reader, Handler &h, const Options &o)
{
  if (!o.enable_follow_refs)
    return;
  foreach (const QString &ref_id, h.h.ref_ids) {
    QFile file(ref2file(ref_id, o));
    validate(file, o);
    QXmlInputSource source(&file);
    bool pret = reader.parse(source);
    if (!pret) {
      QString msg("XML Parse error in referenced file (");
      msg += file.fileName();
      msg += ")";
      throw runtime_error(msg.toUtf8().data());
    }
  }
}

#include "main.h"

void Main::run()
{
  try {
    QStringList arg_list(QCoreApplication::arguments());
    //QStringList arg_list;
    //add_to_list(argc, argv, arg_list);
    Options o;
    o.parse(arg_list);

    QXmlSimpleReader reader;
    Header header;
    Handler h(header);
    parse_main_file(reader, h, o);
    if (o.enable_warnings)
      header.check(cerr);
    header.sort(o);

    parse_refs(reader, h, o);

    if (o.just_dump)
      print_dump(cout, header);
    else {
      o.check_create_output_dir();
      print_man(header, o);
    }

  } catch (const exception &e) {
    cerr << "Exception: " << e.what() << '\n';
    QCoreApplication::exit(1);
    return;
    //return 1;
  }
  QCoreApplication::quit();
}

int main(int argc, char **argv)
{
  // Eventloop needed for QXmlSchemaValidator
  QCoreApplication app(argc, argv);
  Main m;
  QTimer::singleShot(0, &m, SLOT(run()));
  return app.exec();
  //return 0;
}

