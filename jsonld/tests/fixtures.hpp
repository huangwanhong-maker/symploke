// Made by make_fixtures.py with pyld ?; do not edit by hand.
#pragma once

#include <map>
#include <string>
#include <vector>

namespace fixtures {

struct Case { std::string name, input, base; std::map<std::string, std::string> documents; std::string expected; };

inline const std::vector<Case> to_rdf = {
    {R"fx(terms, prefixes, types and a default language)fx", R"fx({"@context": {"ex": "http://example.org/vocab#", "foaf": "http://xmlns.com/foaf/0.1/", "name": "foaf:name", "homepage": {"@id": "foaf:homepage", "@type": "@id"}, "@language": "en"}, "@id": "http://example.org/people/ada", "@type": ["foaf:Person", "ex:Mathematician"], "name": "Ada", "homepage": "http://example.org/ada/", "ex:nick": {"@value": "Ada", "@language": "fr"}, "ex:untagged": {"@value": "plain", "@language": null}})fx", R"fx(http://example.org/doc)fx", {}, R"fx(<http://example.org/people/ada> <http://example.org/vocab#nick> "Ada"@fr .
<http://example.org/people/ada> <http://example.org/vocab#untagged> "plain" .
<http://example.org/people/ada> <http://www.w3.org/1999/02/22-rdf-syntax-ns#type> <http://example.org/vocab#Mathematician> .
<http://example.org/people/ada> <http://www.w3.org/1999/02/22-rdf-syntax-ns#type> <http://xmlns.com/foaf/0.1/Person> .
<http://example.org/people/ada> <http://xmlns.com/foaf/0.1/homepage> <http://example.org/ada/> .
<http://example.org/people/ada> <http://xmlns.com/foaf/0.1/name> "Ada"@en .
)fx"},
    {R"fx(the vocabulary and the base, relative IRIs resolved)fx", R"fx({"@context": {"@vocab": "http://example.org/vocab#", "@base": "http://example.org/base/"}, "@id": "item/1", "@type": "Item", "part": {"@id": "../other#x"}, "title": "One", "rel": {"@id": "?q=1"}})fx", R"fx(http://example.org/doc)fx", {}, R"fx(<http://example.org/base/item/1> <http://example.org/vocab#part> <http://example.org/other#x> .
<http://example.org/base/item/1> <http://example.org/vocab#rel> <http://example.org/base/?q=1> .
<http://example.org/base/item/1> <http://example.org/vocab#title> "One" .
<http://example.org/base/item/1> <http://www.w3.org/1999/02/22-rdf-syntax-ns#type> <http://example.org/vocab#Item> .
)fx"},
    {R"fx(type coercion: identifiers, vocabulary terms, datatypes and JSON)fx", R"fx({"@context": {"ex": "http://example.org/vocab#", "xsd": "http://www.w3.org/2001/XMLSchema#", "date": {"@id": "ex:date", "@type": "xsd:date"}, "see": {"@id": "ex:see", "@type": "@id"}, "kind": {"@id": "ex:kind", "@type": "@vocab"}, "Big": "ex:Big", "data": {"@id": "ex:data", "@type": "@json"}}, "@id": "http://example.org/x", "date": "1931-04-01", "see": "y", "kind": "Big", "data": {"b": [1, 2.5, true, null], "a": "€", "c": {"z": 1, "y": "x"}}})fx", R"fx(http://example.org/doc)fx", {}, R"fx(<http://example.org/x> <http://example.org/vocab#data> "{\"a\":\"€\",\"b\":[1,2.5,true,null],\"c\":{\"y\":\"x\",\"z\":1}}"^^<http://www.w3.org/1999/02/22-rdf-syntax-ns#JSON> .
<http://example.org/x> <http://example.org/vocab#date> "1931-04-01"^^<http://www.w3.org/2001/XMLSchema#date> .
<http://example.org/x> <http://example.org/vocab#kind> <http://example.org/vocab#Big> .
<http://example.org/x> <http://example.org/vocab#see> <http://example.org/y> .
)fx"},
    {R"fx(numbers and booleans by the API's rule)fx", R"fx({"@context": {"ex": "http://example.org/vocab#", "xsd": "http://www.w3.org/2001/XMLSchema#"}, "@id": "http://example.org/n", "ex:i": 1, "ex:f": 1.5, "ex:whole": 5.0, "ex:huge": 1e+21, "ex:yes": true, "ex:big": 12345678901234567890, "ex:neg": -3.25e-05, "ex:typed": {"@value": 3, "@type": "xsd:double"}, "ex:dec": {"@value": "2.50", "@type": "xsd:decimal"}})fx", R"fx(http://example.org/doc)fx", {}, R"fx(<http://example.org/n> <http://example.org/vocab#big> "12345678901234567890"^^<http://www.w3.org/2001/XMLSchema#integer> .
<http://example.org/n> <http://example.org/vocab#dec> "2.50"^^<http://www.w3.org/2001/XMLSchema#decimal> .
<http://example.org/n> <http://example.org/vocab#f> "1.5E0"^^<http://www.w3.org/2001/XMLSchema#double> .
<http://example.org/n> <http://example.org/vocab#huge> "1.0E21"^^<http://www.w3.org/2001/XMLSchema#double> .
<http://example.org/n> <http://example.org/vocab#i> "1"^^<http://www.w3.org/2001/XMLSchema#integer> .
<http://example.org/n> <http://example.org/vocab#neg> "-3.25E-5"^^<http://www.w3.org/2001/XMLSchema#double> .
<http://example.org/n> <http://example.org/vocab#typed> "3.0E0"^^<http://www.w3.org/2001/XMLSchema#double> .
<http://example.org/n> <http://example.org/vocab#whole> "5"^^<http://www.w3.org/2001/XMLSchema#integer> .
<http://example.org/n> <http://example.org/vocab#yes> "true"^^<http://www.w3.org/2001/XMLSchema#boolean> .
)fx"},
    {R"fx(lists, sets, empty lists and lists of lists)fx", R"fx({"@context": {"ex": "http://example.org/vocab#", "seq": {"@id": "ex:seq", "@container": "@list"}, "tags": {"@id": "ex:tags", "@container": "@set"}}, "@id": "http://example.org/l", "seq": ["a", "b", {"@id": "http://example.org/c"}], "tags": ["x", "y"], "ex:empty": {"@list": []}, "ex:nested": {"@list": [[1, 2], [3]]}, "ex:one": {"@set": ["solo"]}})fx", R"fx(http://example.org/doc)fx", {}, R"fx(<http://example.org/l> <http://example.org/vocab#empty> <http://www.w3.org/1999/02/22-rdf-syntax-ns#nil> .
<http://example.org/l> <http://example.org/vocab#nested> _:b0 .
<http://example.org/l> <http://example.org/vocab#one> "solo" .
<http://example.org/l> <http://example.org/vocab#seq> _:b5 .
<http://example.org/l> <http://example.org/vocab#tags> "x" .
<http://example.org/l> <http://example.org/vocab#tags> "y" .
_:b0 <http://www.w3.org/1999/02/22-rdf-syntax-ns#first> _:b1 .
_:b0 <http://www.w3.org/1999/02/22-rdf-syntax-ns#rest> _:b3 .
_:b1 <http://www.w3.org/1999/02/22-rdf-syntax-ns#first> "1"^^<http://www.w3.org/2001/XMLSchema#integer> .
_:b1 <http://www.w3.org/1999/02/22-rdf-syntax-ns#rest> _:b2 .
_:b2 <http://www.w3.org/1999/02/22-rdf-syntax-ns#first> "2"^^<http://www.w3.org/2001/XMLSchema#integer> .
_:b2 <http://www.w3.org/1999/02/22-rdf-syntax-ns#rest> <http://www.w3.org/1999/02/22-rdf-syntax-ns#nil> .
_:b3 <http://www.w3.org/1999/02/22-rdf-syntax-ns#first> _:b4 .
_:b3 <http://www.w3.org/1999/02/22-rdf-syntax-ns#rest> <http://www.w3.org/1999/02/22-rdf-syntax-ns#nil> .
_:b4 <http://www.w3.org/1999/02/22-rdf-syntax-ns#first> "3"^^<http://www.w3.org/2001/XMLSchema#integer> .
_:b4 <http://www.w3.org/1999/02/22-rdf-syntax-ns#rest> <http://www.w3.org/1999/02/22-rdf-syntax-ns#nil> .
_:b5 <http://www.w3.org/1999/02/22-rdf-syntax-ns#first> "a" .
_:b5 <http://www.w3.org/1999/02/22-rdf-syntax-ns#rest> _:b6 .
_:b6 <http://www.w3.org/1999/02/22-rdf-syntax-ns#first> "b" .
_:b6 <http://www.w3.org/1999/02/22-rdf-syntax-ns#rest> _:b7 .
_:b7 <http://www.w3.org/1999/02/22-rdf-syntax-ns#first> <http://example.org/c> .
_:b7 <http://www.w3.org/1999/02/22-rdf-syntax-ns#rest> <http://www.w3.org/1999/02/22-rdf-syntax-ns#nil> .
)fx"},
    {R"fx(language maps and term languages)fx", R"fx({"@context": {"ex": "http://example.org/vocab#", "@language": "de", "label": {"@id": "ex:label", "@container": "@language"}, "code": {"@id": "ex:code", "@language": null}, "note": {"@id": "ex:note", "@language": "ja"}}, "@id": "http://example.org/m", "label": {"en": "Reading room", "fr": ["Salle", "de lecture"], "@none": "untagged"}, "code": "R1", "note": "閲覧室", "ex:plain": "Lesesaal"})fx", R"fx(http://example.org/doc)fx", {}, R"fx(<http://example.org/m> <http://example.org/vocab#code> "R1" .
<http://example.org/m> <http://example.org/vocab#label> "Reading room"@en .
<http://example.org/m> <http://example.org/vocab#label> "Salle"@fr .
<http://example.org/m> <http://example.org/vocab#label> "de lecture"@fr .
<http://example.org/m> <http://example.org/vocab#label> "untagged" .
<http://example.org/m> <http://example.org/vocab#note> "閲覧室"@ja .
<http://example.org/m> <http://example.org/vocab#plain> "Lesesaal"@de .
)fx"},
    {R"fx(index maps, property-valued indexes, id maps and type maps)fx", R"fx({"@context": {"ex": "http://example.org/vocab#", "dcterms": "http://purl.org/dc/terms/", "byIndex": {"@id": "ex:byIndex", "@container": "@index"}, "byCode": {"@id": "ex:byCode", "@container": "@index", "@index": "dcterms:identifier"}, "byId": {"@id": "ex:byId", "@container": "@id"}, "byType": {"@id": "ex:byType", "@container": "@type"}}, "@id": "http://example.org/i", "byIndex": {"first": {"@id": "http://example.org/a"}, "second": "a value"}, "byCode": {"F-1": {"@id": "http://example.org/fonds"}, "@none": {"@id": "http://example.org/nocode"}}, "byId": {"http://example.org/p": {"ex:name": "P"}, "_:q": {"ex:name": "Q"}}, "byType": {"ex:Letter": {"@id": "http://example.org/letter"}, "@none": {"@id": "http://example.org/untyped"}}})fx", R"fx(http://example.org/doc)fx", {}, R"fx(<http://example.org/fonds> <http://purl.org/dc/terms/identifier> "F-1" .
<http://example.org/i> <http://example.org/vocab#byCode> <http://example.org/fonds> .
<http://example.org/i> <http://example.org/vocab#byCode> <http://example.org/nocode> .
<http://example.org/i> <http://example.org/vocab#byId> <http://example.org/p> .
<http://example.org/i> <http://example.org/vocab#byId> _:b0 .
<http://example.org/i> <http://example.org/vocab#byIndex> "a value" .
<http://example.org/i> <http://example.org/vocab#byIndex> <http://example.org/a> .
<http://example.org/i> <http://example.org/vocab#byType> <http://example.org/letter> .
<http://example.org/i> <http://example.org/vocab#byType> <http://example.org/untyped> .
<http://example.org/letter> <http://www.w3.org/1999/02/22-rdf-syntax-ns#type> <http://example.org/vocab#Letter> .
<http://example.org/p> <http://example.org/vocab#name> "P" .
_:b0 <http://example.org/vocab#name> "Q" .
)fx"},
    {R"fx(graph containers: plain, with ids and with indexes)fx", R"fx({"@context": {"ex": "http://example.org/vocab#", "claims": {"@id": "ex:claims", "@container": "@graph"}, "byGraph": {"@id": "ex:byGraph", "@container": ["@graph", "@id"]}, "byIdx": {"@id": "ex:byIdx", "@container": ["@graph", "@index"]}}, "@id": "http://example.org/g", "claims": {"@id": "http://example.org/s", "ex:says": "so"}, "byGraph": {"http://example.org/g1": {"@id": "http://example.org/s1", "ex:p": "one"}}, "byIdx": {"k": {"@id": "http://example.org/s2", "ex:p": "two"}}})fx", R"fx(http://example.org/doc)fx", {}, R"fx(<http://example.org/g> <http://example.org/vocab#byGraph> <http://example.org/g1> .
<http://example.org/g> <http://example.org/vocab#byIdx> _:b0 .
<http://example.org/g> <http://example.org/vocab#claims> _:b1 .
<http://example.org/s1> <http://example.org/vocab#p> "one" <http://example.org/g1> .
<http://example.org/s2> <http://example.org/vocab#p> "two" _:b0 .
<http://example.org/s> <http://example.org/vocab#says> "so" _:b1 .
)fx"},
    {R"fx(reverse properties, by term and by keyword)fx", R"fx({"@context": {"ex": "http://example.org/vocab#", "partOf": {"@reverse": "ex:hasPart"}}, "@id": "http://example.org/item", "partOf": {"@id": "http://example.org/series"}, "@reverse": {"ex:holds": {"@id": "http://example.org/box"}}})fx", R"fx(http://example.org/doc)fx", {}, R"fx(<http://example.org/box> <http://example.org/vocab#holds> <http://example.org/item> .
<http://example.org/series> <http://example.org/vocab#hasPart> <http://example.org/item> .
)fx"},
    {R"fx(nesting and inclusion)fx", R"fx({"@context": {"ex": "http://example.org/vocab#", "meta": "@nest", "label": {"@id": "ex:label", "@nest": "meta"}}, "@id": "http://example.org/n1", "meta": {"label": "nested", "ex:more": "too"}, "@included": [{"@id": "http://example.org/n2", "ex:label": "included"}]})fx", R"fx(http://example.org/doc)fx", {}, R"fx(<http://example.org/n1> <http://example.org/vocab#label> "nested" .
<http://example.org/n1> <http://example.org/vocab#more> "too" .
<http://example.org/n2> <http://example.org/vocab#label> "included" .
)fx"},
    {R"fx(scoped contexts on properties and on types)fx", R"fx({"@context": {"ex": "http://example.org/vocab#", "other": "http://example.org/other#", "author": {"@id": "ex:author", "@context": {"name": "other:fullName"}}, "Letter": {"@id": "ex:Letter", "@context": {"title": "other:letterTitle"}}, "Book": {"@id": "ex:Book", "@context": {"@propagate": true, "title": "other:bookTitle"}}, "name": "ex:name", "title": "ex:title", "about": {"@id": "ex:about"}}, "@graph": [{"@id": "http://example.org/b1", "name": "top", "author": {"@id": "http://example.org/a1", "name": "scoped"}}, {"@id": "http://example.org/l1", "@type": "Letter", "title": "a letter", "about": {"@id": "http://example.org/t1", "title": "not the letter's"}}, {"@id": "http://example.org/k1", "@type": "Book", "title": "a book", "about": {"@id": "http://example.org/t2", "title": "the book's still"}}]})fx", R"fx(http://example.org/doc)fx", {}, R"fx(<http://example.org/a1> <http://example.org/other#fullName> "scoped" .
<http://example.org/b1> <http://example.org/vocab#author> <http://example.org/a1> .
<http://example.org/b1> <http://example.org/vocab#name> "top" .
<http://example.org/k1> <http://example.org/other#bookTitle> "a book" .
<http://example.org/k1> <http://example.org/vocab#about> <http://example.org/t2> .
<http://example.org/k1> <http://www.w3.org/1999/02/22-rdf-syntax-ns#type> <http://example.org/vocab#Book> .
<http://example.org/l1> <http://example.org/other#letterTitle> "a letter" .
<http://example.org/l1> <http://example.org/vocab#about> <http://example.org/t1> .
<http://example.org/l1> <http://www.w3.org/1999/02/22-rdf-syntax-ns#type> <http://example.org/vocab#Letter> .
<http://example.org/t1> <http://example.org/vocab#title> "not the letter's" .
<http://example.org/t2> <http://example.org/other#bookTitle> "the book's still" .
)fx"},
    {R"fx(remote and imported contexts, given by IRI)fx", R"fx({"@context": ["http://example.org/contexts/shared.jsonld", {"@import": "http://example.org/contexts/imported.jsonld", "kind": {"@id": "ex:kindOf", "@type": "@vocab"}}], "@id": "http://example.org/r", "name": "Remote", "knows": "http://example.org/s", "label": "imported", "kind": "ex:K"})fx", R"fx(http://example.org/doc)fx", {{R"fx(http://example.org/contexts/shared.jsonld)fx", R"fx({"@context": {"schema": "https://schema.org/", "name": "schema:name", "knows": {"@id": "schema:knows", "@type": "@id"}}})fx"}, {R"fx(http://example.org/contexts/imported.jsonld)fx", R"fx({"@context": {"ex": "http://example.org/vocab#", "label": "ex:label", "kind": {"@id": "ex:kind", "@type": "@vocab"}}})fx"}}, R"fx(<http://example.org/r> <http://example.org/vocab#kindOf> <http://example.org/vocab#K> .
<http://example.org/r> <http://example.org/vocab#label> "imported" .
<http://example.org/r> <https://schema.org/knows> <http://example.org/s> .
<http://example.org/r> <https://schema.org/name> "Remote" .
)fx"},
    {R"fx(protected terms kept when redefined alike)fx", R"fx({"@context": [{"@protected": true, "ex": "http://example.org/vocab#", "title": "ex:title"}, {"title": "ex:title"}], "@id": "http://example.org/pt", "title": "kept"})fx", R"fx(http://example.org/doc)fx", {}, R"fx(<http://example.org/pt> <http://example.org/vocab#title> "kept" .
)fx"},
    {R"fx(named graphs and blank nodes)fx", R"fx({"@context": {"ex": "http://example.org/vocab#"}, "@graph": [{"@id": "http://example.org/graphs/one", "@graph": [{"@id": "_:a", "ex:p": {"@id": "_:b"}}, {"@id": "_:b", "ex:q": "v"}]}, {"@id": "http://example.org/top", "ex:anon": {"ex:inner": {"ex:deeper": "x"}}, "ex:labelled": {"@id": "_:a"}}]})fx", R"fx(http://example.org/doc)fx", {}, R"fx(<http://example.org/top> <http://example.org/vocab#anon> _:b2 .
<http://example.org/top> <http://example.org/vocab#labelled> _:b0 .
_:b0 <http://example.org/vocab#p> _:b1 <http://example.org/graphs/one> .
_:b1 <http://example.org/vocab#q> "v" <http://example.org/graphs/one> .
_:b2 <http://example.org/vocab#inner> _:b3 .
_:b3 <http://example.org/vocab#deeper> "x" .
)fx"},
    {R"fx(what has no IRI yields no statement)fx", R"fx({"@context": {"ex": "http://example.org/vocab#"}, "@id": "relative-subject", "ex:p": "dropped", "ex:q": {"@id": "http://example.org/kept", "ex:r": "kept"}, "_:blankpredicate": "dropped"})fx", R"fx()fx", {}, R"fx(<http://example.org/kept> <http://example.org/vocab#r> "kept" .
)fx"},
    {R"fx(a Web Annotation, its context given)fx", R"fx({"@context": "http://example.org/contexts/anno.jsonld", "id": "http://example.org/anno/1", "type": "Annotation", "motivation": "commenting", "creator": "http://example.org/people/brandt", "created": "2026-11-15T10:00:00Z", "body": {"type": "TextualBody", "value": "The map cases.", "format": "text/plain", "language": "en"}, "target": {"type": "SpecificResource", "source": "http://example.org/holdings/plan", "selector": {"type": "FragmentSelector", "conformsTo": "http://www.w3.org/TR/media-frags/", "value": "xywh=10,20,300,200"}}})fx", R"fx(http://example.org/doc)fx", {{R"fx(http://example.org/contexts/anno.jsonld)fx", R"fx({"@context": {"oa": "http://www.w3.org/ns/oa#", "dc": "http://purl.org/dc/elements/1.1/", "dcterms": "http://purl.org/dc/terms/", "xsd": "http://www.w3.org/2001/XMLSchema#", "rdf": "http://www.w3.org/1999/02/22-rdf-syntax-ns#", "id": {"@type": "@id", "@id": "@id"}, "type": {"@type": "@id", "@id": "@type"}, "Annotation": "oa:Annotation", "TextualBody": "oa:TextualBody", "SpecificResource": "oa:SpecificResource", "FragmentSelector": "oa:FragmentSelector", "TextQuoteSelector": "oa:TextQuoteSelector", "body": {"@type": "@id", "@id": "oa:hasBody"}, "target": {"@type": "@id", "@id": "oa:hasTarget"}, "source": {"@type": "@id", "@id": "oa:hasSource"}, "selector": {"@type": "@id", "@id": "oa:hasSelector"}, "value": "rdf:value", "exact": "oa:exact", "prefix": "oa:prefix", "suffix": "oa:suffix", "conformsTo": {"@type": "@id", "@id": "dcterms:conformsTo"}, "creator": {"@type": "@id", "@id": "dcterms:creator"}, "created": {"@id": "dcterms:created", "@type": "xsd:dateTime"}, "motivation": {"@type": "@vocab", "@id": "oa:motivatedBy"}, "commenting": "oa:commenting", "tagging": "oa:tagging", "language": "dc:language", "format": "dc:format"}})fx"}}, R"fx(<http://example.org/anno/1> <http://purl.org/dc/terms/created> "2026-11-15T10:00:00Z"^^<http://www.w3.org/2001/XMLSchema#dateTime> .
<http://example.org/anno/1> <http://purl.org/dc/terms/creator> <http://example.org/people/brandt> .
<http://example.org/anno/1> <http://www.w3.org/1999/02/22-rdf-syntax-ns#type> <http://www.w3.org/ns/oa#Annotation> .
<http://example.org/anno/1> <http://www.w3.org/ns/oa#hasBody> _:b0 .
<http://example.org/anno/1> <http://www.w3.org/ns/oa#hasTarget> _:b1 .
<http://example.org/anno/1> <http://www.w3.org/ns/oa#motivatedBy> <http://www.w3.org/ns/oa#commenting> .
_:b0 <http://purl.org/dc/elements/1.1/format> "text/plain" .
_:b0 <http://purl.org/dc/elements/1.1/language> "en" .
_:b0 <http://www.w3.org/1999/02/22-rdf-syntax-ns#type> <http://www.w3.org/ns/oa#TextualBody> .
_:b0 <http://www.w3.org/1999/02/22-rdf-syntax-ns#value> "The map cases." .
_:b1 <http://www.w3.org/1999/02/22-rdf-syntax-ns#type> <http://www.w3.org/ns/oa#SpecificResource> .
_:b1 <http://www.w3.org/ns/oa#hasSelector> _:b2 .
_:b1 <http://www.w3.org/ns/oa#hasSource> <http://example.org/holdings/plan> .
_:b2 <http://purl.org/dc/terms/conformsTo> <http://www.w3.org/TR/media-frags/> .
_:b2 <http://www.w3.org/1999/02/22-rdf-syntax-ns#type> <http://www.w3.org/ns/oa#FragmentSelector> .
_:b2 <http://www.w3.org/1999/02/22-rdf-syntax-ns#value> "xywh=10,20,300,200" .
)fx"},
    {R"fx(an array of contexts and a null to reset)fx", R"fx({"@context": [{"ex": "http://example.org/vocab#", "name": "ex:name"}, null, {"other": "http://example.org/other#"}], "@id": "http://example.org/reset", "name": "no longer a term", "other:p": "kept"})fx", R"fx(http://example.org/doc)fx", {}, R"fx(<http://example.org/reset> <http://example.org/other#p> "kept" .
)fx"},
};

inline const std::vector<Case> errors = {
    {R"fx(a term defined by a number)fx", R"fx({"@context": {"t": 1}, "@id": "http://example.org/e", "t": "x"})fx", R"fx(http://example.org/doc)fx", {}, R"fx(invalid term definition)fx"},
    {R"fx(a cycle of compact IRIs)fx", R"fx({"@context": {"a": "b:x", "b": "a:y"}, "@id": "http://example.org/e", "a": "x"})fx", R"fx(http://example.org/doc)fx", {}, R"fx(cyclic IRI mapping)fx"},
    {R"fx(colliding keywords)fx", R"fx({"@context": {"id": "@id"}, "@id": "http://example.org/e1", "id": "http://example.org/e2"})fx", R"fx(http://example.org/doc)fx", {}, R"fx(colliding keywords)fx"},
    {R"fx(an @id that is not a string)fx", R"fx({"@id": 5, "http://example.org/p": "x"})fx", R"fx(http://example.org/doc)fx", {}, R"fx(invalid @id value)fx"},
    {R"fx(a value with both a type and a language)fx", R"fx({"@id": "http://example.org/e", "http://example.org/p": {"@value": "x", "@language": "en", "@type": "http://example.org/t"}})fx", R"fx(http://example.org/doc)fx", {}, R"fx(invalid value object)fx"},
    {R"fx(a remote context not given)fx", R"fx({"@context": "http://example.org/contexts/missing.jsonld", "@id": "http://example.org/e"})fx", R"fx(http://example.org/doc)fx", {}, R"fx(loading remote context failed)fx"},
    {R"fx(a list that is also a set)fx", R"fx({"@context": {"t": {"@id": "http://example.org/t", "@container": ["@list", "@set"]}}, "@id": "http://example.org/e", "t": [1]})fx", R"fx(http://example.org/doc)fx", {}, R"fx(invalid container mapping)fx"},
    {R"fx(a keyword redefined)fx", R"fx({"@context": {"@id": "http://example.org/id"}, "http://example.org/p": "x"})fx", R"fx(http://example.org/doc)fx", {}, R"fx(keyword redefinition)fx"},
    {R"fx(a protected term redefined otherwise)fx", R"fx({"@context": [{"@protected": true, "t": "http://example.org/t"}, {"t": "http://example.org/u"}], "@id": "http://example.org/e", "t": "x"})fx", R"fx(http://example.org/doc)fx", {}, R"fx(protected term redefinition)fx"},
    {R"fx(a typed value whose type is not an IRI)fx", R"fx({"@id": "http://example.org/e", "http://example.org/p": {"@value": "x", "@type": "_:notiri"}})fx", R"fx(http://example.org/doc)fx", {}, R"fx(invalid typed value)fx"},
    {R"fx(a context of the wrong kind)fx", R"fx({"@context": 5, "@id": "http://example.org/e"})fx", R"fx(http://example.org/doc)fx", {}, R"fx(invalid local context)fx"},
    {R"fx(a language map holding a number)fx", R"fx({"@context": {"t": {"@id": "http://example.org/t", "@container": "@language"}}, "@id": "http://example.org/e", "t": {"en": 5}})fx", R"fx(http://example.org/doc)fx", {}, R"fx(invalid language map value)fx"},
};

struct Compacted { std::string name, nquads; std::map<std::string, std::string> prefixes; std::string expected; };

inline const std::vector<Compacted> compact = {
    {R"fx(a holding described)fx", R"fx(<urn:archive:p:plan> <http://www.w3.org/1999/02/22-rdf-syntax-ns#type> <http://purl.org/dc/dcmitype/StillImage> .
<urn:archive:p:plan> <http://purl.org/dc/terms/title> "Plan of the reading room" .
<urn:archive:p:plan> <http://purl.org/dc/terms/title> "Plan de la salle de lecture"@fr .
<urn:archive:p:plan> <http://purl.org/dc/terms/date> "1931"^^<http://www.w3.org/2001/XMLSchema#gYear> .
<urn:archive:p:plan> <http://purl.org/dc/terms/creator> <urn:archive:p:brandt> .
<urn:archive:p:plan> <http://example.net/unprefixed#p> "no prefix fits" .
<urn:archive:p:brandt> <http://xmlns.com/foaf/0.1/name> "L. Brandt" .
)fx", {{R"fx(dcterms)fx", R"fx(http://purl.org/dc/terms/)fx"}, {R"fx(dcmitype)fx", R"fx(http://purl.org/dc/dcmitype/)fx"}, {R"fx(foaf)fx", R"fx(http://xmlns.com/foaf/0.1/)fx"}, {R"fx(xsd)fx", R"fx(http://www.w3.org/2001/XMLSchema#)fx"}, {R"fx(a)fx", R"fx(urn:archive:p:)fx"}}, R"fx({"@context": {"dcterms": "http://purl.org/dc/terms/", "dcmitype": "http://purl.org/dc/dcmitype/", "foaf": "http://xmlns.com/foaf/0.1/", "xsd": "http://www.w3.org/2001/XMLSchema#", "a": "urn:archive:p:"}, "@graph": [{"@id": "a:brandt", "foaf:name": "L. Brandt"}, {"@id": "a:plan", "@type": "dcmitype:StillImage", "http://example.net/unprefixed#p": "no prefix fits", "dcterms:creator": {"@id": "a:brandt"}, "dcterms:date": {"@type": "xsd:gYear", "@value": "1931"}, "dcterms:title": ["Plan of the reading room", {"@language": "fr", "@value": "Plan de la salle de lecture"}]}]})fx"},
    {R"fx(a list and an empty list)fx", R"fx(<http://example.org/s> <http://example.org/v#seq> _:l1 .
_:l1 <http://www.w3.org/1999/02/22-rdf-syntax-ns#first> "one" .
_:l1 <http://www.w3.org/1999/02/22-rdf-syntax-ns#rest> _:l2 .
_:l2 <http://www.w3.org/1999/02/22-rdf-syntax-ns#first> <http://example.org/two> .
_:l2 <http://www.w3.org/1999/02/22-rdf-syntax-ns#rest> <http://www.w3.org/1999/02/22-rdf-syntax-ns#nil> .
<http://example.org/s> <http://example.org/v#none> <http://www.w3.org/1999/02/22-rdf-syntax-ns#nil> .
)fx", {{R"fx(v)fx", R"fx(http://example.org/v#)fx"}, {R"fx(ex)fx", R"fx(http://example.org/)fx"}}, R"fx({"@context": {"v": "http://example.org/v#", "ex": "http://example.org/"}, "@id": "ex:s", "v:none": {"@list": []}, "v:seq": {"@list": ["one", {"@id": "ex:two"}]}})fx"},
    {R"fx(a named graph beside the default)fx", R"fx(<http://example.org/a> <http://example.org/v#p> "default" .
<http://example.org/b> <http://example.org/v#p> "named" <http://example.org/g> .
<http://example.org/c> <http://example.org/v#q> _:x <http://example.org/g> .
_:x <http://example.org/v#p> "blank in a graph" <http://example.org/g> .
)fx", {{R"fx(v)fx", R"fx(http://example.org/v#)fx"}}, R"fx({"@context": {"v": "http://example.org/v#"}, "@graph": [{"@id": "http://example.org/a", "v:p": "default"}, {"@graph": [{"@id": "_:x", "v:p": "blank in a graph"}, {"@id": "http://example.org/b", "v:p": "named"}, {"@id": "http://example.org/c", "v:q": {"@id": "_:x"}}], "@id": "http://example.org/g"}]})fx"},
    {R"fx(one node and a JSON literal)fx", R"fx(<http://example.org/one> <http://example.org/v#data> "{\"a\":[1,2],\"b\":true}"^^<http://www.w3.org/1999/02/22-rdf-syntax-ns#JSON> .
<http://example.org/one> <http://example.org/v#n> "5"^^<http://www.w3.org/2001/XMLSchema#integer> .
)fx", {{R"fx(v)fx", R"fx(http://example.org/v#)fx"}, {R"fx(xsd)fx", R"fx(http://www.w3.org/2001/XMLSchema#)fx"}}, R"fx({"@context": {"v": "http://example.org/v#", "xsd": "http://www.w3.org/2001/XMLSchema#"}, "@id": "http://example.org/one", "v:data": {"@type": "@json", "@value": {"a": [1, 2], "b": true}}, "v:n": {"@type": "xsd:integer", "@value": "5"}})fx"},
};

}  // namespace fixtures
