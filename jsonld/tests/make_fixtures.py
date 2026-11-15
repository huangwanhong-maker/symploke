"""The reference values of check_symploke_jsonld, made by pyld (Digital Bazaar's JSON-LD processor for Python, the reference
implementation of the W3C's JSON-LD 1.1 API; BSD licence), never written by hand.

For each document below, pyld's toRdf gives the statements (N-Quads) or the error code the document must be refused with; for
each dataset, pyld's fromRdf and compaction against a context of prefixes give the document a writer must agree with. The cases
cover every keyword of JSON-LD 1.1 that symploke reads. Remote contexts are given to pyld by a document loader that knows only
the documents listed with each case, as symploke is given them.

    python -m venv env && env/bin/pip install pyld
    env/bin/python jsonld/tests/make_fixtures.py      (from symploke's root; writes jsonld/tests/fixtures.hpp)
"""

from __future__ import annotations

import copy
import json
from pathlib import Path

import pyld
from pyld import jsonld

HERE = Path(__file__).resolve().parent
BASE = "http://example.org/doc"

ANNO_CONTEXT = {"@context": {
    "oa": "http://www.w3.org/ns/oa#", "dc": "http://purl.org/dc/elements/1.1/", "dcterms": "http://purl.org/dc/terms/",
    "xsd": "http://www.w3.org/2001/XMLSchema#", "rdf": "http://www.w3.org/1999/02/22-rdf-syntax-ns#",
    "id": {"@type": "@id", "@id": "@id"}, "type": {"@type": "@id", "@id": "@type"},
    "Annotation": "oa:Annotation", "TextualBody": "oa:TextualBody", "SpecificResource": "oa:SpecificResource",
    "FragmentSelector": "oa:FragmentSelector", "TextQuoteSelector": "oa:TextQuoteSelector",
    "body": {"@type": "@id", "@id": "oa:hasBody"}, "target": {"@type": "@id", "@id": "oa:hasTarget"},
    "source": {"@type": "@id", "@id": "oa:hasSource"}, "selector": {"@type": "@id", "@id": "oa:hasSelector"},
    "value": "rdf:value", "exact": "oa:exact", "prefix": "oa:prefix", "suffix": "oa:suffix",
    "conformsTo": {"@type": "@id", "@id": "dcterms:conformsTo"}, "creator": {"@type": "@id", "@id": "dcterms:creator"},
    "created": {"@id": "dcterms:created", "@type": "xsd:dateTime"},
    "motivation": {"@type": "@vocab", "@id": "oa:motivatedBy"}, "commenting": "oa:commenting", "tagging": "oa:tagging",
    "language": "dc:language", "format": "dc:format"}}

SHARED = {"@context": {"schema": "https://schema.org/", "name": "schema:name", "knows": {"@id": "schema:knows", "@type": "@id"}}}

IMPORTED = {"@context": {"ex": "http://example.org/vocab#", "label": "ex:label", "kind": {"@id": "ex:kind", "@type": "@vocab"}}}

TO_RDF = [
    ("terms, prefixes, types and a default language", {
        "@context": {"ex": "http://example.org/vocab#", "foaf": "http://xmlns.com/foaf/0.1/", "name": "foaf:name",
                     "homepage": {"@id": "foaf:homepage", "@type": "@id"}, "@language": "en"},
        "@id": "http://example.org/people/ada", "@type": ["foaf:Person", "ex:Mathematician"],
        "name": "Ada", "homepage": "http://example.org/ada/", "ex:nick": {"@value": "Ada", "@language": "fr"},
        "ex:untagged": {"@value": "plain", "@language": None}}, {}),
    ("the vocabulary and the base, relative IRIs resolved", {
        "@context": {"@vocab": "http://example.org/vocab#", "@base": "http://example.org/base/"},
        "@id": "item/1", "@type": "Item", "part": {"@id": "../other#x"}, "title": "One", "rel": {"@id": "?q=1"}}, {}),
    ("type coercion: identifiers, vocabulary terms, datatypes and JSON", {
        "@context": {"ex": "http://example.org/vocab#", "xsd": "http://www.w3.org/2001/XMLSchema#",
                     "date": {"@id": "ex:date", "@type": "xsd:date"}, "see": {"@id": "ex:see", "@type": "@id"},
                     "kind": {"@id": "ex:kind", "@type": "@vocab"}, "Big": "ex:Big",
                     "data": {"@id": "ex:data", "@type": "@json"}},
        "@id": "http://example.org/x", "date": "1931-04-01", "see": "y", "kind": "Big",
        "data": {"b": [1, 2.5, True, None], "a": "€", "c": {"z": 1, "y": "x"}}}, {}),
    ("numbers and booleans by the API's rule", {
        "@context": {"ex": "http://example.org/vocab#", "xsd": "http://www.w3.org/2001/XMLSchema#"},
        "@id": "http://example.org/n", "ex:i": 1, "ex:f": 1.5, "ex:whole": 5.0, "ex:huge": 1e21, "ex:yes": True,
        "ex:big": 12345678901234567890, "ex:neg": -3.25e-5, "ex:typed": {"@value": 3, "@type": "xsd:double"},
        "ex:dec": {"@value": "2.50", "@type": "xsd:decimal"}}, {}),
    ("lists, sets, empty lists and lists of lists", {
        "@context": {"ex": "http://example.org/vocab#", "seq": {"@id": "ex:seq", "@container": "@list"},
                     "tags": {"@id": "ex:tags", "@container": "@set"}},
        "@id": "http://example.org/l", "seq": ["a", "b", {"@id": "http://example.org/c"}], "tags": ["x", "y"],
        "ex:empty": {"@list": []}, "ex:nested": {"@list": [[1, 2], [3]]}, "ex:one": {"@set": ["solo"]}}, {}),
    ("language maps and term languages", {
        "@context": {"ex": "http://example.org/vocab#", "@language": "de",
                     "label": {"@id": "ex:label", "@container": "@language"},
                     "code": {"@id": "ex:code", "@language": None}, "note": {"@id": "ex:note", "@language": "ja"}},
        "@id": "http://example.org/m", "label": {"en": "Reading room", "fr": ["Salle", "de lecture"], "@none": "untagged"},
        "code": "R1", "note": "閲覧室", "ex:plain": "Lesesaal"}, {}),
    ("index maps, property-valued indexes, id maps and type maps", {
        "@context": {"ex": "http://example.org/vocab#", "dcterms": "http://purl.org/dc/terms/",
                     "byIndex": {"@id": "ex:byIndex", "@container": "@index"},
                     "byCode": {"@id": "ex:byCode", "@container": "@index", "@index": "dcterms:identifier"},
                     "byId": {"@id": "ex:byId", "@container": "@id"}, "byType": {"@id": "ex:byType", "@container": "@type"}},
        "@id": "http://example.org/i", "byIndex": {"first": {"@id": "http://example.org/a"}, "second": "a value"},
        "byCode": {"F-1": {"@id": "http://example.org/fonds"}, "@none": {"@id": "http://example.org/nocode"}},
        "byId": {"http://example.org/p": {"ex:name": "P"}, "_:q": {"ex:name": "Q"}},
        "byType": {"ex:Letter": {"@id": "http://example.org/letter"}, "@none": {"@id": "http://example.org/untyped"}}}, {}),
    ("graph containers: plain, with ids and with indexes", {
        "@context": {"ex": "http://example.org/vocab#", "claims": {"@id": "ex:claims", "@container": "@graph"},
                     "byGraph": {"@id": "ex:byGraph", "@container": ["@graph", "@id"]},
                     "byIdx": {"@id": "ex:byIdx", "@container": ["@graph", "@index"]}},
        "@id": "http://example.org/g", "claims": {"@id": "http://example.org/s", "ex:says": "so"},
        "byGraph": {"http://example.org/g1": {"@id": "http://example.org/s1", "ex:p": "one"}},
        "byIdx": {"k": {"@id": "http://example.org/s2", "ex:p": "two"}}}, {}),
    ("reverse properties, by term and by keyword", {
        "@context": {"ex": "http://example.org/vocab#", "partOf": {"@reverse": "ex:hasPart"}},
        "@id": "http://example.org/item", "partOf": {"@id": "http://example.org/series"},
        "@reverse": {"ex:holds": {"@id": "http://example.org/box"}}}, {}),
    ("nesting and inclusion", {
        "@context": {"ex": "http://example.org/vocab#", "meta": "@nest", "label": {"@id": "ex:label", "@nest": "meta"}},
        "@id": "http://example.org/n1", "meta": {"label": "nested", "ex:more": "too"},
        "@included": [{"@id": "http://example.org/n2", "ex:label": "included"}]}, {}),
    ("scoped contexts on properties and on types", {
        "@context": {"ex": "http://example.org/vocab#", "other": "http://example.org/other#",
                     "author": {"@id": "ex:author", "@context": {"name": "other:fullName"}},
                     "Letter": {"@id": "ex:Letter", "@context": {"title": "other:letterTitle"}},
                     "Book": {"@id": "ex:Book", "@context": {"@propagate": True, "title": "other:bookTitle"}},
                     "name": "ex:name", "title": "ex:title", "about": {"@id": "ex:about"}},
        "@graph": [
            {"@id": "http://example.org/b1", "name": "top", "author": {"@id": "http://example.org/a1", "name": "scoped"}},
            {"@id": "http://example.org/l1", "@type": "Letter", "title": "a letter",
             "about": {"@id": "http://example.org/t1", "title": "not the letter's"}},
            {"@id": "http://example.org/k1", "@type": "Book", "title": "a book",
             "about": {"@id": "http://example.org/t2", "title": "the book's still"}}]}, {}),
    ("remote and imported contexts, given by IRI", {
        "@context": ["http://example.org/contexts/shared.jsonld", {"@import": "http://example.org/contexts/imported.jsonld",
                                                                   "kind": {"@id": "ex:kindOf", "@type": "@vocab"}}],
        "@id": "http://example.org/r", "name": "Remote", "knows": "http://example.org/s", "label": "imported", "kind": "ex:K"},
     {"http://example.org/contexts/shared.jsonld": SHARED, "http://example.org/contexts/imported.jsonld": IMPORTED}),
    ("protected terms kept when redefined alike", {
        "@context": [{"@protected": True, "ex": "http://example.org/vocab#", "title": "ex:title"},
                     {"title": "ex:title"}],
        "@id": "http://example.org/pt", "title": "kept"}, {}),
    ("named graphs and blank nodes", {
        "@context": {"ex": "http://example.org/vocab#"},
        "@graph": [
            {"@id": "http://example.org/graphs/one", "@graph": [{"@id": "_:a", "ex:p": {"@id": "_:b"}}, {"@id": "_:b", "ex:q": "v"}]},
            {"@id": "http://example.org/top", "ex:anon": {"ex:inner": {"ex:deeper": "x"}}, "ex:labelled": {"@id": "_:a"}}]}, {}),
    ("what has no IRI yields no statement", {
        "@context": {"ex": "http://example.org/vocab#"},
        "@id": "relative-subject", "ex:p": "dropped", "ex:q": {"@id": "http://example.org/kept", "ex:r": "kept"},
        "_:blankpredicate": "dropped"}, {}, ""),
    ("a Web Annotation, its context given", {
        "@context": "http://example.org/contexts/anno.jsonld",
        "id": "http://example.org/anno/1", "type": "Annotation", "motivation": "commenting",
        "creator": "http://example.org/people/brandt", "created": "2026-11-15T10:00:00Z",
        "body": {"type": "TextualBody", "value": "The map cases.", "format": "text/plain", "language": "en"},
        "target": {"type": "SpecificResource", "source": "http://example.org/holdings/plan",
                   "selector": {"type": "FragmentSelector", "conformsTo": "http://www.w3.org/TR/media-frags/", "value": "xywh=10,20,300,200"}}},
     {"http://example.org/contexts/anno.jsonld": ANNO_CONTEXT}),
    ("an array of contexts and a null to reset", {
        "@context": [{"ex": "http://example.org/vocab#", "name": "ex:name"}, None, {"other": "http://example.org/other#"}],
        "@id": "http://example.org/reset", "name": "no longer a term", "other:p": "kept"}, {}),
]

ERRORS = [
    ("a term defined by a number", {"@context": {"t": 1}, "@id": "http://example.org/e", "t": "x"}, {}),
    ("a cycle of compact IRIs", {"@context": {"a": "b:x", "b": "a:y"}, "@id": "http://example.org/e", "a": "x"}, {}),
    ("colliding keywords", {"@context": {"id": "@id"}, "@id": "http://example.org/e1", "id": "http://example.org/e2"}, {}),
    ("an @id that is not a string", {"@id": 5, "http://example.org/p": "x"}, {}),
    ("a value with both a type and a language", {"@id": "http://example.org/e", "http://example.org/p": {"@value": "x", "@language": "en", "@type": "http://example.org/t"}}, {}),
    ("a remote context not given", {"@context": "http://example.org/contexts/missing.jsonld", "@id": "http://example.org/e"}, {}),
    ("a list that is also a set", {"@context": {"t": {"@id": "http://example.org/t", "@container": ["@list", "@set"]}}, "@id": "http://example.org/e", "t": [1]}, {}),
    ("a keyword redefined", {"@context": {"@id": "http://example.org/id"}, "http://example.org/p": "x"}, {}),
    ("a protected term redefined otherwise", {"@context": [{"@protected": True, "t": "http://example.org/t"}, {"t": "http://example.org/u"}], "@id": "http://example.org/e", "t": "x"}, {}),
    ("a typed value whose type is not an IRI", {"@id": "http://example.org/e", "http://example.org/p": {"@value": "x", "@type": "_:notiri"}}, {}),
    ("a context of the wrong kind", {"@context": 5, "@id": "http://example.org/e"}, {}),
    ("a language map holding a number", {"@context": {"t": {"@id": "http://example.org/t", "@container": "@language"}}, "@id": "http://example.org/e", "t": {"en": 5}}, {}),
]

COMPACT = [
    ("a holding described", """<urn:archive:p:plan> <http://www.w3.org/1999/02/22-rdf-syntax-ns#type> <http://purl.org/dc/dcmitype/StillImage> .
<urn:archive:p:plan> <http://purl.org/dc/terms/title> "Plan of the reading room" .
<urn:archive:p:plan> <http://purl.org/dc/terms/title> "Plan de la salle de lecture"@fr .
<urn:archive:p:plan> <http://purl.org/dc/terms/date> "1931"^^<http://www.w3.org/2001/XMLSchema#gYear> .
<urn:archive:p:plan> <http://purl.org/dc/terms/creator> <urn:archive:p:brandt> .
<urn:archive:p:plan> <http://example.net/unprefixed#p> "no prefix fits" .
<urn:archive:p:brandt> <http://xmlns.com/foaf/0.1/name> "L. Brandt" .
""", {"dcterms": "http://purl.org/dc/terms/", "dcmitype": "http://purl.org/dc/dcmitype/", "foaf": "http://xmlns.com/foaf/0.1/",
      "xsd": "http://www.w3.org/2001/XMLSchema#", "a": "urn:archive:p:"}),
    ("a list and an empty list", """<http://example.org/s> <http://example.org/v#seq> _:l1 .
_:l1 <http://www.w3.org/1999/02/22-rdf-syntax-ns#first> "one" .
_:l1 <http://www.w3.org/1999/02/22-rdf-syntax-ns#rest> _:l2 .
_:l2 <http://www.w3.org/1999/02/22-rdf-syntax-ns#first> <http://example.org/two> .
_:l2 <http://www.w3.org/1999/02/22-rdf-syntax-ns#rest> <http://www.w3.org/1999/02/22-rdf-syntax-ns#nil> .
<http://example.org/s> <http://example.org/v#none> <http://www.w3.org/1999/02/22-rdf-syntax-ns#nil> .
""", {"v": "http://example.org/v#", "ex": "http://example.org/"}),
    ("a named graph beside the default", """<http://example.org/a> <http://example.org/v#p> "default" .
<http://example.org/b> <http://example.org/v#p> "named" <http://example.org/g> .
<http://example.org/c> <http://example.org/v#q> _:x <http://example.org/g> .
_:x <http://example.org/v#p> "blank in a graph" <http://example.org/g> .
""", {"v": "http://example.org/v#"}),
    ("one node and a JSON literal", """<http://example.org/one> <http://example.org/v#data> "{\\"a\\":[1,2],\\"b\\":true}"^^<http://www.w3.org/1999/02/22-rdf-syntax-ns#JSON> .
<http://example.org/one> <http://example.org/v#n> "5"^^<http://www.w3.org/2001/XMLSchema#integer> .
""", {"v": "http://example.org/v#", "xsd": "http://www.w3.org/2001/XMLSchema#"}),
]


def loader_for(documents):
    def load(url, options=None):
        if url not in documents:
            raise jsonld.JsonLdError("not given", "loading document failed", {"url": url})
        return {"contentType": "application/ld+json", "contextUrl": None, "documentUrl": url, "document": documents[url]}
    return load


def code_of(error):
    e = error
    while e is not None:
        code = getattr(e, "code", None)
        if code and not str(code).startswith("jsonld."):
            return str(code)
        e = getattr(e, "cause", None) or getattr(e, "__cause__", None)
    return getattr(error, "code", "unknown")


def raw(s: str) -> str:
    assert ')fx"' not in s
    return 'R"fx(' + s + ')fx"'


def main() -> None:
    out = ["// Made by make_fixtures.py with pyld " + getattr(pyld, "__version__", "?") + "; do not edit by hand.", "#pragma once", "",
           "#include <map>", "#include <string>", "#include <vector>", "", "namespace fixtures {", "",
           "struct Case { std::string name, input, base; std::map<std::string, std::string> documents; std::string expected; };", "",
           "inline const std::vector<Case> to_rdf = {"]
    for name, doc, docs, *rest in TO_RDF:
        base = rest[0] if rest else BASE
        # pyld puts a base of its own (http://example.org/base/) in place of an empty one; None means no base, as the API says.
        opts = {"base": base or None, "format": "application/n-quads", "documentLoader": loader_for(docs)}
        # Written before pyld sees them: pyld writes a key of its own (_uuid) into the contexts it is given.
        documents = "{" + ", ".join("{" + raw(k) + ", " + raw(json.dumps(v)) + "}" for k, v in docs.items()) + "}"
        opts["documentLoader"] = loader_for(copy.deepcopy(docs))
        nq = jsonld.to_rdf(copy.deepcopy(doc), opts)
        out.append("    {" + raw(name) + ", " + raw(json.dumps(doc, ensure_ascii=False)) + ", " + raw(base) + ", " + documents + ", " + raw(nq) + "},")
    out += ["};", "", "inline const std::vector<Case> errors = {"]
    for name, doc, docs in ERRORS:
        try:
            jsonld.expand(doc, {"base": BASE, "documentLoader": loader_for(docs)})
            code = "none"
        except jsonld.JsonLdError as e:
            code = code_of(e)
        out.append("    {" + raw(name) + ", " + raw(json.dumps(doc)) + ", " + raw(BASE) + ", {}, " + raw(code) + "},")
    out += ["};", "", "struct Compacted { std::string name, nquads; std::map<std::string, std::string> prefixes; std::string expected; };", "",
            "inline const std::vector<Compacted> compact = {"]
    for name, nq, prefixes in COMPACT:
        expanded = jsonld.from_rdf(nq, {"format": "application/n-quads"})
        compacted = jsonld.compact(expanded, {"@context": prefixes})
        p = "{" + ", ".join("{" + raw(k) + ", " + raw(v) + "}" for k, v in prefixes.items()) + "}"
        out.append("    {" + raw(name) + ", " + raw(nq) + ", " + p + ", " + raw(json.dumps(compacted, ensure_ascii=False)) + "},")
    out += ["};", "", "}  // namespace fixtures", ""]
    (HERE / "fixtures.hpp").write_text("\n".join(out), encoding="utf-8")
    print("wrote", HERE / "fixtures.hpp")


if __name__ == "__main__":
    main()
