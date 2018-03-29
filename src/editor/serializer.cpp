// This file is part of Dementia.
// Copyright (C) 2018 Jussi Lind <jussi.lind@iki.fi>
//
// Dementia is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// Dementia is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Dementia. If not, see <http://www.gnu.org/licenses/>.

#include "serializer.hpp"
#include "config.hpp"
#include "node.hpp"

#include <QDomElement>

static const double SCALE = 1000; // https://bugreports.qt.io/browse/QTBUG-67129

using std::make_shared;

static void writeNodes(MindMapData & mindMapData, QDomElement & root, QDomDocument & doc)
{
    for (auto && nodeIter : mindMapData.graph())
    {
        QDomElement nodeElement = doc.createElement(Serializer::DataKeywords::Graph::NODE);
        nodeElement.setAttribute(Serializer::DataKeywords::Graph::Node::INDEX, nodeIter.second->index());
        nodeElement.setAttribute(Serializer::DataKeywords::Graph::Node::X, static_cast<int>(nodeIter.second->location().x() * SCALE));
        nodeElement.setAttribute(Serializer::DataKeywords::Graph::Node::Y, static_cast<int>(nodeIter.second->location().y() * SCALE));
        root.appendChild(nodeElement);

        // Create a child node for the text content
        QDomElement textElement = doc.createElement(Serializer::DataKeywords::Graph::Node::TEXT);
        nodeElement.appendChild(textElement);
        QDomText textNode = doc.createTextNode(nodeIter.second->text());
        textElement.appendChild(textNode);
    }
}

static QString readFirstTextNodeContent(QDomElement element)
{
    for (int i = 0; i < element.childNodes().count(); i++)
    {
        const auto child = element.childNodes().at(i);
        if (child.isText())
        {
            return child.toText().nodeValue();
        }
    }
    return "";
}

// The purpose of this #ifdef is to build GUILESS unit tests so that QTEST_GUILESS_MAIN can be used
#ifdef DEMENTIA_UNIT_TEST
static NodeBasePtr readNode(const QDomElement & element)
#else
static NodePtr readNode(const QDomElement & element)
#endif
{
#ifdef DEMENTIA_UNIT_TEST
    auto node = make_shared<NodeBase>();
#else
    // Init a new node. QGraphicsScene will take the ownership eventually.
    auto node = make_shared<Node>();
#endif
    node->setIndex(element.attribute(Serializer::DataKeywords::Graph::Node::INDEX, "-1").toInt());
    node->setLocation(QPointF(
        element.attribute(Serializer::DataKeywords::Graph::Node::X, "0").toInt() / SCALE,
        element.attribute(Serializer::DataKeywords::Graph::Node::Y, "0").toInt() / SCALE));

    for (int i = 0; i < element.childNodes().count(); i++)
    {
        const auto child = element.childNodes().at(i);
        if (child.isElement())
        {
            const auto element = child.toElement();
            if (element.nodeName() == Serializer::DataKeywords::Graph::Node::TEXT)
            {
                node->setText(readFirstTextNodeContent(element));
            }
        }
    }

    return node;
}

Serializer::Serializer(MindMapData & mindMapData)
    : m_mindMapData(mindMapData)
{
    HashSeed::init(); // Already set in main(), but this is for unit tests
}

MindMapDataPtr Serializer::fromXml(QDomDocument document)
{
    const QDomElement root = document.documentElement();
    const auto version = root.attribute(DataKeywords::Header::APPLICATION_VERSION, "UNDEFINED");

    auto data = make_shared<MindMapData>();
    data->setVersion(version);

    QDomNode domNode = root.firstChild();
    while (!domNode.isNull())
    {
        QDomElement element = domNode.toElement();
        if (!element.isNull())
        {
            // Read a node element
            if (element.nodeName() == DataKeywords::Graph::NODE)
            {
                auto node = readNode(element);
                data->graph().addNode(node);
            }
        }

        domNode = domNode.nextSibling();
    }

    return data;
}

QDomDocument Serializer::toXml()
{
    QDomDocument doc;
    QDomElement root = doc.createElement(Serializer::DataKeywords::Header::DESIGN);

    root.setAttribute(Serializer::DataKeywords::Header::APPLICATION_VERSION, Config::APPLICATION_VERSION);
    doc.appendChild(root);

    writeNodes(m_mindMapData, root, doc);

    return doc;
}
