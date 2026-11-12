#include "fdx.h"

#include <cstdlib>
#include <sstream>
#include <vector>

namespace {

std::string xmlEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            default: out.push_back(c);
        }
    }
    return out;
}

std::string xmlUnescape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] != '&') { out.push_back(s[i]); continue; }
        size_t semi = s.find(';', i);
        if (semi == std::string::npos || semi - i > 10) { out.push_back('&'); continue; }
        std::string ent = s.substr(i + 1, semi - i - 1);
        if (ent == "amp") out += '&';
        else if (ent == "lt") out += '<';
        else if (ent == "gt") out += '>';
        else if (ent == "quot") out += '"';
        else if (ent == "apos") out += '\'';
        else if (!ent.empty() && ent[0] == '#') {
            long cp = (ent.size() > 1 && (ent[1] == 'x' || ent[1] == 'X'))
                          ? std::strtol(ent.c_str() + 2, nullptr, 16)
                          : std::strtol(ent.c_str() + 1, nullptr, 10);
            if (cp > 0) out += u32ToUtf8(static_cast<char32_t>(cp));
        } else {
            out += s.substr(i, semi - i + 1);
        }
        i = semi;
    }
    return out;
}

std::string attribute(const std::string& tag, const std::string& name) {
    std::string key = name + "=\"";
    size_t p = tag.find(key);
    if (p == std::string::npos) return {};
    p += key.size();
    size_t e = tag.find('"', p);
    return e == std::string::npos ? std::string{} : xmlUnescape(tag.substr(p, e - p));
}

Element elementFromFdx(const std::string& t) {
    if (t == "Scene Heading") return Element::SceneHeading;
    if (t == "Character") return Element::Character;
    if (t == "Parenthetical") return Element::Parenthetical;
    if (t == "Dialogue") return Element::Dialogue;
    if (t == "Transition") return Element::Transition;
    return Element::Action;  // Action, General, Shot, etc.
}

}  // namespace

std::string writeFdx(const Document& doc) {
    std::ostringstream o;
    o << "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\" ?>\n";
    o << "<FinalDraft DocumentType=\"Script\" Template=\"No\" Version=\"5\">\n";
    o << "  <Content>\n";
    const std::vector<int> sides = dualSides(doc);
    const size_t n = doc.paragraphs.size();
    for (size_t i = 0; i < n; ++i) {
        const Paragraph& p = doc.paragraphs[i];
        // Diálogo duplo: as duas falas vão juntas dentro de <DualDialogue>.
        if (sides[i] == 1 && (i == 0 || sides[i - 1] == 0))
            o << "    <Paragraph>\n      <DualDialogue>\n";
        o << "    <Paragraph Type=\"" << elementFdxName(p.type) << "\">\n";
        if (p.type == Element::SceneHeading && !p.synopsis.empty()) {
            o << "      <SceneProperties>\n        <Summary>\n";
            o << "          <Paragraph Type=\"Action\"><Text>" << xmlEscape(u32ToUtf8(p.synopsis))
              << "</Text></Paragraph>\n";
            o << "        </Summary>\n      </SceneProperties>\n";
        }
        o << "      <Text>" << xmlEscape(u32ToUtf8(p.text)) << "</Text>\n";
        o << "    </Paragraph>\n";
        if (sides[i] == 2 && (i + 1 == n || sides[i + 1] != 2))
            o << "      </DualDialogue>\n    </Paragraph>\n";
    }
    o << "  </Content>\n";

    if (!doc.titlePage.empty()) {
        o << "  <TitlePage>\n    <Content>\n";
        auto para = [&](const std::string& text, const char* align) {
            o << "      <Paragraph Alignment=\"" << align << "\" Type=\"Text\">\n";
            o << "        <Text>" << xmlEscape(text) << "</Text>\n";
            o << "      </Paragraph>\n";
        };
        for (const char* key : {"Title", "Credit", "Author"}) {
            std::string v = doc.titleValue(key);
            if (v.empty()) continue;
            std::istringstream vs(v);
            std::string line;
            while (std::getline(vs, line)) para(line, "Center");
            para("", "Center");
        }
        std::string contact = doc.titleValue("Contact");
        std::istringstream cs(contact);
        std::string line;
        while (std::getline(cs, line)) para(line, "Left");
        o << "    </Content>\n  </TitlePage>\n";
    }
    o << "</FinalDraft>\n";
    return o.str();
}

Document parseFdx(const std::string& xml) {
    Document doc;
    size_t cStart = xml.find("<Content");
    if (cStart == std::string::npos) { doc.ensureNotEmpty(); return doc; }
    size_t cEnd = xml.find("</Content>", cStart);
    if (cEnd == std::string::npos) cEnd = xml.size();

    size_t pos = cStart;
    bool inDual = false;
    int dualCues = 0;
    while (true) {
        size_t p = xml.find("<Paragraph", pos);
        // Fim de um bloco <DualDialogue> antes do próximo parágrafo.
        if (inDual) {
            size_t dEnd = xml.find("</DualDialogue>", pos);
            if (dEnd != std::string::npos && (p == std::string::npos || dEnd < p)) {
                inDual = false;
                size_t outer = xml.find("</Paragraph>", dEnd);
                pos = outer == std::string::npos ? dEnd + 15 : outer + 12;
                continue;
            }
        }
        if (p == std::string::npos || p >= cEnd) break;
        size_t tagEnd = xml.find('>', p);
        if (tagEnd == std::string::npos) break;
        std::string tag = xml.substr(p, tagEnd - p + 1);
        // <Paragraph><DualDialogue>...: os parágrafos de dentro são lidos normalmente.
        size_t dd = xml.find("<DualDialogue", tagEnd);
        size_t nextPara = xml.find("<Paragraph", tagEnd);
        if (dd != std::string::npos && dd < nextPara && xml.find("<Text", tagEnd) > dd) {
            inDual = true;
            dualCues = 0;
            pos = xml.find('>', dd) + 1;
            continue;
        }
        Paragraph para;
        para.type = elementFromFdx(attribute(tag, "Type"));
        if (tag.size() >= 2 && tag[tag.size() - 2] == '/') {  // <Paragraph ... />
            doc.paragraphs.push_back(para);
            pos = tagEnd + 1;
            continue;
        }
        // <SceneProperties> pode conter parágrafos aninhados (a sinopse); trata antes.
        size_t searchFrom = tagEnd;
        std::string summary;
        size_t sp = xml.find("<SceneProperties", tagEnd);
        size_t firstClose = xml.find("</Paragraph>", tagEnd);
        if (sp != std::string::npos && sp < firstClose) {
            size_t spEnd = xml.find("</SceneProperties>", sp);
            if (spEnd != std::string::npos) {
                std::string props = xml.substr(sp, spEnd - sp);
                size_t t = 0;
                while ((t = props.find("<Text", t)) != std::string::npos) {
                    size_t te = props.find('>', t);
                    size_t tc = props.find("</Text>", te);
                    if (te == std::string::npos || tc == std::string::npos) break;
                    if (!summary.empty()) summary += " ";
                    summary += xmlUnescape(props.substr(te + 1, tc - te - 1));
                    t = tc + 7;
                }
                searchFrom = spEnd + 18;
            }
        }
        size_t close = xml.find("</Paragraph>", searchFrom);
        if (close == std::string::npos) close = cEnd;
        std::string body = xml.substr(searchFrom == tagEnd ? tagEnd + 1 : searchFrom,
                                      close - (searchFrom == tagEnd ? tagEnd + 1 : searchFrom));
        para.synopsis = utf8ToU32(summary);
        std::string text;
        size_t t = 0;
        while ((t = body.find("<Text", t)) != std::string::npos) {
            size_t te = body.find('>', t);
            if (te == std::string::npos) break;
            if (body[te - 1] == '/') { t = te + 1; continue; }
            size_t tc = body.find("</Text>", te);
            if (tc == std::string::npos) break;
            text += xmlUnescape(body.substr(te + 1, tc - te - 1));
            t = tc + 7;
        }
        para.text = utf8ToU32(text);
        if (inDual && para.type == Element::Character) para.dual = ++dualCues >= 2;
        doc.paragraphs.push_back(para);
        pos = close + 12;
    }

    // Página de rosto: título = primeira linha centralizada não vazia.
    size_t tp = xml.find("<TitlePage");
    if (tp != std::string::npos) {
        std::vector<std::string> centered;
        size_t q = tp;
        while ((q = xml.find("<Text", q)) != std::string::npos) {
            size_t te = xml.find('>', q);
            size_t tc = xml.find("</Text>", te);
            if (te == std::string::npos || tc == std::string::npos) break;
            std::string s = xmlUnescape(xml.substr(te + 1, tc - te - 1));
            if (!s.empty()) centered.push_back(s);
            q = tc + 7;
        }
        if (!centered.empty()) doc.setTitleValue("Title", centered[0]);
        if (centered.size() > 2) {
            doc.setTitleValue("Credit", centered[1]);
            doc.setTitleValue("Author", centered[2]);
        }
    }
    doc.ensureNotEmpty();
    return doc;
}
