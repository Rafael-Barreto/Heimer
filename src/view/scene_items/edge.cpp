// This file is part of Heimer.
// Copyright (C) 2018 Jussi Lind <jussi.lind@iki.fi>
//
// Heimer is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// Heimer is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Heimer. If not, see <http://www.gnu.org/licenses/>.

#include "edge.hpp"

#include "../../application/service_container.hpp"
#include "../../application/settings_proxy.hpp"
#include "../../common/test_mode.hpp"
#include "../../domain/graph.hpp"
#include "../shadow_effect_params.hpp"
#include "edge_dot.hpp"
#include "edge_text_edit.hpp"
#include "graphics_factory.hpp"
#include "layers.hpp"
#include "node.hpp"

#include "simple_logger.hpp"

#include <QBrush>
#include <QFont>
#include <QGraphicsEllipseItem>
#include <QGraphicsLineItem>
#include <QGraphicsScene>
#include <QPen>
#include <QPropertyAnimation>
#include <QTimer>
#include <QVector2D>

#include <QtMath>

#include <cmath>

namespace SceneItems {

Edge::Edge(NodeP sourceNode, NodeP targetNode, bool enableAnimations, bool enableLabel)
  : m_settingsProxy(SC::instance().settingsProxy())
  , m_edgeModel(std::make_unique<EdgeModel>(m_settingsProxy->reversedEdgeDirection(),
                                            EdgeModel::Style { m_settingsProxy->edgeArrowMode() }))
  , m_sourceNode(sourceNode)
  , m_targetNode(targetNode)
  , m_enableAnimations(enableAnimations)
  , m_enableLabel(enableLabel)
  , m_sourceDot(enableAnimations ? new EdgeDot(this) : nullptr)
  , m_targetDot(enableAnimations ? new EdgeDot(this) : nullptr)
  , m_label(enableLabel ? new EdgeTextEdit(this) : nullptr)
  , m_dummyLabel(enableLabel ? new EdgeTextEdit(this) : nullptr)
  , m_line(new QGraphicsLineItem(this))
  , m_arrowheadBeginLeft(new QGraphicsLineItem(this))
  , m_arrowheadBeginRight(new QGraphicsLineItem(this))
  , m_arrowheadEndLeft(new QGraphicsLineItem(this))
  , m_arrowheadEndRight(new QGraphicsLineItem(this))
  , m_sourceDotSizeAnimation(enableAnimations ? new QPropertyAnimation(m_sourceDot, "scale", this) : nullptr)
  , m_targetDotSizeAnimation(enableAnimations ? new QPropertyAnimation(m_targetDot, "scale", this) : nullptr)
{
    setAcceptHoverEvents(true && enableAnimations);

    setGraphicsEffect(GraphicsFactory::createDropShadowEffect(m_settingsProxy->shadowEffect(), false));

    setZValue(static_cast<int>(Layers::Edge));

    initDots();

    if (m_enableLabel) {

        const QColor labelColor { 0xff, 0xee, 0xaa };

        m_label->setZValue(static_cast<int>(Layers::EdgeLabel));
        m_label->setBackgroundColor(labelColor);
        connect(m_label, &TextEdit::textChanged, this, [=](const QString & text) {
            updateLabel();
            m_edgeModel->text = text;
        });

        connect(m_label, &TextEdit::undoPointRequested, this, &Edge::undoPointRequested);

        m_dummyLabel->setZValue(static_cast<int>(Layers::EdgeDummyLabel));
        m_dummyLabel->setAcceptHoverEvents(false);
        m_dummyLabel->setBackgroundColor(labelColor);
        m_dummyLabel->setText(tr("..."));
        m_dummyLabel->setEnabled(false);

        connect(m_label, &EdgeTextEdit::hoverEntered, this, [=] {
            setLabelVisible(true, EdgeTextEdit::VisibilityChangeReason::Focused);
        });

        connect(m_label, &EdgeTextEdit::visibilityTimeout, this, [=] {
            setLabelVisible(false);
        });

        m_labelVisibilityTimer.setSingleShot(true);

        const int labelDurationMs = 2000;

        m_labelVisibilityTimer.setInterval(labelDurationMs);

        connect(&m_labelVisibilityTimer, &QTimer::timeout, this, [=] {
            setLabelVisible(false);
        });
    }
}

Edge::Edge(NodeS sourceNode, NodeS targetNode, bool enableAnimations, bool enableLabel)
  : Edge(sourceNode.get(), targetNode.get(), enableAnimations, enableLabel)
{
}

Edge::Edge(EdgeCR other, GraphCR graph)
  : Edge(graph.getNode(other.m_sourceNode->index()).get(), graph.getNode(other.m_targetNode->index()).get())
{
    copyData(other);
}

Edge::Edge(EdgeCR other)
  : Edge(nullptr, nullptr)
{
    copyData(other);
}

void Edge::hoverEnterEvent(QGraphicsSceneHoverEvent * event)
{
    m_labelVisibilityTimer.stop();

    setLabelVisible(true, EdgeTextEdit::VisibilityChangeReason::Focused);

    QGraphicsItem::hoverEnterEvent(event);
}

void Edge::hoverLeaveEvent(QGraphicsSceneHoverEvent * event)
{
    m_labelVisibilityTimer.start();

    QGraphicsItem::hoverLeaveEvent(event);
}

QPen Edge::buildPen(bool ignoreDashSetting) const
{
    QPen pen { QBrush { QColor { m_color.red(), m_color.green(), m_color.blue() } }, m_edgeModel->style.edgeWidth };
    pen.setCapStyle(Qt::PenCapStyle::RoundCap);
    if (!ignoreDashSetting && m_edgeModel->style.dashedLine) {
        pen.setDashPattern({ qreal(5), qreal(5) });
    }
    return pen;
}

void Edge::copyData(EdgeCR other)
{
    *m_edgeModel = *other.m_edgeModel;

    setText(other.m_edgeModel->text); // Update text to the label component
}

void Edge::changeFont(const QFont & font)
{
    if (m_enableLabel) {
        // Handle size and family separately to maintain backwards compatibility
        QFont newFont(font);
        if (m_label->font().pointSize() >= 0) {
            newFont.setPointSize(m_label->font().pointSize());
        }
        m_label->setFont(newFont);
        m_dummyLabel->setFont(newFont);
    }
}

bool Edge::dashedLine() const
{
    return m_edgeModel->style.dashedLine;
}

void Edge::enableShadowEffect(bool enable)
{
    GraphicsFactory::updateDropShadowEffect(graphicsEffect(), m_settingsProxy->shadowEffect(), m_selected, !enable);
    update();
}

void Edge::highlightText(const QString & text)
{
    if (!TestMode::enabled()) {
        m_label->selectText(text);
    } else {
        TestMode::logDisabledCode("highlightText");
    }
}

QString Edge::id() const
{
    const auto unknown = "??";
    return QString { "%1_%2" }.arg(m_sourceNode ? QString::number(m_sourceNode->index()) : unknown, m_targetNode ? QString::number(m_targetNode->index()) : unknown);
}

double Edge::length() const
{
    return m_line->line().length();
}

void Edge::initDots()
{
    if (m_enableAnimations) {

        const QColor dotColor { 255, 0, 0, 192 };

        m_sourceDot->setPen(QPen(dotColor));
        m_sourceDot->setBrush(QBrush(dotColor));
        m_sourceDot->setZValue(zValue() + 10);

        m_targetDot->setPen(QPen(dotColor));
        m_targetDot->setBrush(QBrush(dotColor));
        m_targetDot->setZValue(zValue() + 10);

        const int dotDurationMs = 2000;

        m_sourceDotSizeAnimation->setDuration(dotDurationMs);
        m_sourceDotSizeAnimation->setStartValue(1.0);
        m_sourceDotSizeAnimation->setEndValue(0.0);

        const int dotRadius = 10;

        const QRectF rect { -dotRadius, -dotRadius, dotRadius * 2, dotRadius * 2 };

        m_sourceDot->setRect(rect);

        m_targetDotSizeAnimation->setDuration(dotDurationMs);
        m_targetDotSizeAnimation->setStartValue(1.0);
        m_targetDotSizeAnimation->setEndValue(0.0);

        m_targetDot->setRect(rect);
    }
}

void Edge::setArrowHeadPen(const QPen & pen)
{
    m_arrowheadBeginLeft->setPen(pen);
    m_arrowheadBeginLeft->update();
    m_arrowheadBeginRight->setPen(pen);
    m_arrowheadBeginRight->update();
    m_arrowheadEndLeft->setPen(pen);
    m_arrowheadEndLeft->update();
    m_arrowheadEndRight->setPen(pen);
    m_arrowheadEndRight->update();
}

void Edge::setLabelVisible(bool visible, EdgeTextEdit::VisibilityChangeReason vcr)
{
    if (m_enableLabel) {
        // Note: We need to test for scene(), because sceneBoundingRect() will crash if the item has not been added to any scene (yet).
        const bool isEnoughSpaceForLabel = m_label->scene() && !m_label->sceneBoundingRect().intersects(sourceNode().sceneBoundingRect()) && //
          !m_label->sceneBoundingRect().intersects(targetNode().sceneBoundingRect());
        const bool dummyLabelTextIsShoterThanLabelText = m_dummyLabel->text().length() < m_label->text().length();
        const bool isEnoughSpaceForDummyLabel = m_dummyLabel->scene() && !m_dummyLabel->sceneBoundingRect().intersects(sourceNode().sceneBoundingRect()) && //
          !m_dummyLabel->sceneBoundingRect().intersects(targetNode().sceneBoundingRect());
        switch (vcr) {
        case EdgeTextEdit::VisibilityChangeReason::AvailableSpaceChanged: {
            // Toggle visibility according to space available if geometry changed
            const bool isLabelVisible = isEnoughSpaceForLabel && !m_label->text().isEmpty();
            m_label->setVisible(isLabelVisible);
            m_dummyLabel->setVisible(!isLabelVisible && isEnoughSpaceForDummyLabel && dummyLabelTextIsShoterThanLabelText);
        } break;
        case EdgeTextEdit::VisibilityChangeReason::Explicit: {
            m_label->setVisible(visible);
            m_dummyLabel->setVisible(visible);
        } break;
        case EdgeTextEdit::VisibilityChangeReason::Focused: {
            if (visible) {
                m_label->setVisible(true);
                m_label->setParentItem(nullptr);
                m_label->setGraphicsEffect(GraphicsFactory::createDropShadowEffect(m_settingsProxy->shadowEffect(), false));
                m_dummyLabel->setVisible(false);
            }
        } break;
        case EdgeTextEdit::VisibilityChangeReason::Timeout: {
            if (!visible) {
                if ((m_label->text().isEmpty() || (!m_label->text().isEmpty() && !isEnoughSpaceForLabel)) && !m_label->hasFocus()) {
                    m_label->setVisible(false);
                    m_dummyLabel->setVisible(isEnoughSpaceForDummyLabel && dummyLabelTextIsShoterThanLabelText);
                }
            }
        } break;
        }
    }
}

void Edge::setEdgeWidth(double edgeWidth)
{
    m_edgeModel->style.edgeWidth = edgeWidth;

    updateLine();
}

void Edge::setArrowMode(EdgeModel::ArrowMode arrowMode)
{
    m_edgeModel->style.arrowMode = arrowMode;
    if (!TestMode::enabled()) {
        updateLine();
    } else {
        TestMode::logDisabledCode("Update line after arrow mode change");
    }
}

void Edge::setArrowSize(double arrowSize)
{
    m_edgeModel->style.arrowSize = arrowSize;

    updateLine();
}

void Edge::setColor(const QColor & color)
{
    m_color = color;

    updateLine();
}

void Edge::setDashedLine(bool enable)
{
    m_edgeModel->style.dashedLine = enable;
    if (!TestMode::enabled()) {
        updateLine();
    } else {
        TestMode::logDisabledCode("Set dashed line");
    }
}

void Edge::setText(const QString & text)
{
    m_edgeModel->text = text;
    if (m_enableLabel) {
        m_label->setText(text);
        setLabelVisible(!text.isEmpty());
    }
}

void Edge::setTextSize(int textSize)
{
    if (m_enableLabel && textSize > 0) {
        m_label->setTextSize(textSize);
        m_dummyLabel->setTextSize(textSize);
    }
}

void Edge::setReversed(bool reversed)
{
    m_edgeModel->reversed = reversed;

    updateArrowhead();
}

void Edge::setSelected(bool selected)
{
    m_selected = selected;
    GraphicsFactory::updateDropShadowEffect(graphicsEffect(), m_settingsProxy->shadowEffect(), selected);
    if (m_label && m_label->parentItem() != this) {
        GraphicsFactory::updateDropShadowEffect(m_label->graphicsEffect(), m_settingsProxy->shadowEffect(), selected);
    }
    update();
}

void Edge::setShadowEffect(const ShadowEffectParams & params)
{
    GraphicsFactory::updateDropShadowEffect(graphicsEffect(), params, m_selected);
    if (m_label && m_label->parentItem() != this) {
        GraphicsFactory::updateDropShadowEffect(m_label->graphicsEffect(), m_settingsProxy->shadowEffect(), m_selected);
    }
    update();
}

NodeR Edge::sourceNode() const
{
    return *m_sourceNode;
}

NodeR Edge::targetNode() const
{
    return *m_targetNode;
}

void Edge::updateDoubleArrowhead()
{
    QLineF lineBeginLeft, lineBeginRight, lineEndLeft, lineEndRight;

    const double arrowOpening = 150;

    const auto reversedEdge = m_edgeModel->reversed;
    const auto pointBegin = reversedEdge ? m_line->line().p1() : m_line->line().p2();
    lineBeginLeft.setP1(pointBegin);

    const auto angleBegin = reversedEdge ? -m_line->line().angle() + 180 : -m_line->line().angle();
    const auto angleBeginLeft = qDegreesToRadians(angleBegin + arrowOpening);
    lineBeginLeft.setP2(pointBegin + QPointF(std::cos(angleBeginLeft), std::sin(angleBeginLeft)) * m_edgeModel->style.arrowSize);
    lineBeginRight.setP1(pointBegin);

    const auto angleBeginRight = qDegreesToRadians(angleBegin - arrowOpening);
    lineBeginRight.setP2(pointBegin + QPointF(std::cos(angleBeginRight), std::sin(angleBeginRight)) * m_edgeModel->style.arrowSize);
    const auto pointEnd = reversedEdge ? m_line->line().p2() : m_line->line().p1();
    lineEndLeft.setP1(pointEnd);

    m_arrowheadBeginLeft->setLine(lineBeginLeft);
    m_arrowheadBeginRight->setLine(lineBeginRight);
    m_arrowheadBeginLeft->show();
    m_arrowheadBeginRight->show();

    const auto angleEnd = reversedEdge ? -m_line->line().angle() : -m_line->line().angle() + 180;
    const auto angleEndLeft = qDegreesToRadians(angleEnd + arrowOpening);
    lineEndLeft.setP2(pointEnd + QPointF(std::cos(angleEndLeft), std::sin(angleEndLeft)) * m_edgeModel->style.arrowSize);
    lineEndRight.setP1(pointEnd);

    const auto angleEndRight = qDegreesToRadians(angleEnd - arrowOpening);
    lineEndRight.setP2(pointEnd + QPointF(std::cos(angleEndRight), std::sin(angleEndRight)) * m_edgeModel->style.arrowSize);

    m_arrowheadEndLeft->setLine(lineEndLeft);
    m_arrowheadEndRight->setLine(lineEndRight);
    m_arrowheadEndLeft->show();
    m_arrowheadEndRight->show();
}

void Edge::updateHiddenArrowhead()
{
    m_arrowheadBeginLeft->hide();
    m_arrowheadBeginRight->hide();

    m_arrowheadEndLeft->hide();
    m_arrowheadEndRight->hide();
}

void Edge::updateSingleArrowhead()
{
    QLineF lineBeginLeft, lineBeginRight;

    const auto reversedEdge = m_edgeModel->reversed;
    const auto pointBegin = reversedEdge ? m_line->line().p1() : m_line->line().p2();
    lineBeginLeft.setP1(pointBegin);

    const double arrowOpening = 150;
    const auto angleBegin = reversedEdge ? -m_line->line().angle() + 180 : -m_line->line().angle();
    const auto angleLeft = qDegreesToRadians(angleBegin + arrowOpening);
    lineBeginLeft.setP2(pointBegin + QPointF(std::cos(angleLeft), std::sin(angleLeft)) * m_edgeModel->style.arrowSize);
    lineBeginRight.setP1(pointBegin);

    const auto angleRight = qDegreesToRadians(angleBegin - arrowOpening);
    lineBeginRight.setP2(pointBegin + QPointF(std::cos(angleRight), std::sin(angleRight)) * m_edgeModel->style.arrowSize);

    m_arrowheadBeginLeft->setLine(lineBeginLeft);
    m_arrowheadBeginRight->setLine(lineBeginRight);

    m_arrowheadBeginLeft->show();
    m_arrowheadBeginRight->show();

    m_arrowheadEndLeft->hide();
    m_arrowheadEndRight->hide();
}

void Edge::updateArrowhead()
{
    setArrowHeadPen(buildPen(true));

    switch (m_edgeModel->style.arrowMode) {
    case EdgeModel::ArrowMode::Single:
        updateSingleArrowhead();
        break;
    case EdgeModel::ArrowMode::Double:
        updateDoubleArrowhead();
        break;
    case EdgeModel::ArrowMode::Hidden:
        updateHiddenArrowhead();
        break;
    }
}

void Edge::updateDots()
{
    if (m_enableAnimations) {
        // Trigger new animation if relative connection location has changed
        const auto newRelativeSourcePos = m_line->line().p1() - sourceNode().pos();
        if (m_previousRelativeSourcePos != newRelativeSourcePos) {
            m_previousRelativeSourcePos = newRelativeSourcePos;
            m_sourceDotSizeAnimation->stop();
            m_sourceDotSizeAnimation->start();
        }

        // Update location of possibly active animation
        m_sourceDot->setPos(m_line->line().p1());

        // Trigger new animation if relative connection location has changed
        const auto newRelativeTargetPos = m_line->line().p2() - targetNode().pos();
        if (m_previousRelativeTargetPos != newRelativeTargetPos) {
            m_previousRelativeTargetPos = newRelativeTargetPos;
            m_targetDotSizeAnimation->stop();
            m_targetDotSizeAnimation->start();
        }

        // Update location of possibly active animation
        m_targetDot->setPos(m_line->line().p2());
    }
}

void Edge::updateLabel(LabelUpdateReason lur)
{
    if (m_enableLabel) {
        m_label->setPos((m_line->line().p1() + m_line->line().p2()) * 0.5 - QPointF(m_label->boundingRect().width(), m_label->boundingRect().height()) * 0.5);
        m_dummyLabel->setPos((m_line->line().p1() + m_line->line().p2()) * 0.5 - QPointF(m_dummyLabel->boundingRect().width(), m_dummyLabel->boundingRect().height()) * 0.5);
        // Toggle visibility according to space available if geometry changed
        if (lur == LabelUpdateReason::EdgeGeometryChanged) {
            setLabelVisible(m_label->isVisible(), EdgeTextEdit::VisibilityChangeReason::AvailableSpaceChanged);
        }
    }
}

void Edge::setTargetNode(NodeR targetNode)
{
    m_targetNode = &targetNode;
}

void Edge::setSourceNode(NodeR sourceNode)
{
    m_sourceNode = &sourceNode;
}

void Edge::paint(QPainter * painter, const QStyleOptionGraphicsItem * option, QWidget * widget)
{
    // Nothing to do as Edge is just a composite object
    Q_UNUSED(painter)
    Q_UNUSED(option)
    Q_UNUSED(widget)
}

void Edge::removeFromScene()
{
    restoreLabelParent();
    hide();
    if (scene()) {
        scene()->removeItem(this);
    }
}

bool Edge::reversed() const
{
    return m_edgeModel->reversed;
}

void Edge::restoreLabelParent()
{
    if (m_label) {
        m_label->setParentItem(this);
    }
}

bool Edge::selected() const
{
    return m_selected;
}

EdgeModel::ArrowMode Edge::arrowMode() const
{
    return m_edgeModel->style.arrowMode;
}

QRectF Edge::boundingRect() const
{
    return m_line->boundingRect();
}

bool Edge::containsText(const QString & text) const
{
    return m_edgeModel->text.contains(text, Qt::CaseInsensitive);
}

QString Edge::text() const
{
    return m_edgeModel->text;
}

QRectF Edge::translatedLabelBoundingRect() const
{
    return m_label->boundingRect().translated(m_line->line().center());
}

void Edge::unselectText()
{
    m_label->unselectText();
}

void Edge::updateLine()
{
    m_line->setPen(buildPen());

    auto && nearestPoints = Node::getNearestEdgePoints(sourceNode(), targetNode());
    const auto p1 = nearestPoints.first.location + sourceNode().pos();
    QVector2D direction1(sourceNode().pos() - p1);
    direction1.normalize();
    const auto p2 = nearestPoints.second.location + targetNode().pos();
    QVector2D direction2(targetNode().pos() - p2);
    direction2.normalize();

    const double widthScale = 0.5;
    const double cornerRadiusScale = 0.3;
    m_line->setLine(QLineF {
      p1 + (nearestPoints.first.isCorner ? cornerRadiusScale * (direction1 * static_cast<float>(sourceNode().cornerRadius())).toPointF() : QPointF { 0, 0 }),
      p2 + (nearestPoints.second.isCorner ? cornerRadiusScale * (direction2 * static_cast<float>(targetNode().cornerRadius())).toPointF() : QPointF { 0, 0 }) - //
        (direction2 * static_cast<float>(m_edgeModel->style.edgeWidth)).toPointF() * widthScale });

    updateDots();
    updateLabel(LabelUpdateReason::EdgeGeometryChanged);
    updateArrowhead();

    // Set correct origin for scale animations
    setTransformOriginPoint(m_line->line().center());
}

Edge::~Edge()
{
    juzzlin::L().trace() << "Deleting edge (" << (m_sourceNode ? std::to_string(m_sourceNode->index()) : "(none)") << ", " //
                         << (m_targetNode ? std::to_string(m_targetNode->index()) : "(none)") << ")";

    if (!TestMode::enabled()) {
        if (m_enableAnimations) {
            m_sourceDotSizeAnimation->stop();
            m_targetDotSizeAnimation->stop();
        }
        if (m_sourceNode) {
            m_sourceNode->removeGraphicsEdge(*this);
        }
        if (m_targetNode) {
            m_targetNode->removeGraphicsEdge(*this);
        }
    } else {
        TestMode::logDisabledCode("Edge destructor");
    }
}

} // namespace SceneItems
