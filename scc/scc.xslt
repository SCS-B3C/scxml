<?xml version="1.0" encoding="ISO-8859-1"?>
<xsl:stylesheet version="2.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform" xmlns:s="http://www.w3.org/2005/07/scxml" 
    xmlns:Qt="http://www.qtsoftware.com scxml-ext">
<xsl:template name="smname"><xsl:choose><xsl:when test="string(/s:scxml/@name)=''"><xsl:value-of select="$target" /></xsl:when>
 <xsl:otherwise><xsl:value-of select="/sLscxml/@name" /></xsl:otherwise></xsl:choose>
</xsl:template>
<xsl:template name="comment"><xsl:if test="$comments">
        /*
            <xsl:element name="{name()}">
              <xsl:for-each select="@*"><xsl:attribute name="{name()}">
              <xsl:value-of select="string()" /></xsl:attribute></xsl:for-each><xsl:value-of select="text()" /></xsl:element>
        */
</xsl:if></xsl:template>
<xsl:template mode="stateid" match="s:scxml|s:state|s:parallel|s:final|s:initial|s:history" priority="2">
    <xsl:choose>
        <xsl:when test="name()='scxml'">this</xsl:when>
        <xsl:when test="string(@id) != ''" >state_<xsl:value-of select="@id" /></xsl:when>
        <xsl:otherwise>state_<xsl:value-of select="generate-id()" /></xsl:otherwise>
    </xsl:choose>
</xsl:template>
<xsl:template mode="execContext" match="s:if">
<xsl:call-template name="comment" />
              if (<xsl:value-of select="@cond" />) {
  <xsl:apply-templates mode="execContext" />
              }
</xsl:template>
<xsl:template mode="execContext" match="s:else">
<xsl:call-template name="comment" />
              } else {
</xsl:template>
<xsl:template mode="execContext" match="s:elseif">
<xsl:call-template name="comment" />
              } else if (<xsl:value-of select="@cond" />) {
</xsl:template>
<xsl:template mode="execContext" match="s:log">
<xsl:call-template name="comment" />
                QDebug((QtMsgType)<xsl:if test="string(@level)=''">0</xsl:if><xsl:value-of select="@level" />) <xsl:if test="string(@label)!=''">&lt;&lt; "<xsl:value-of select="@label" />" </xsl:if>&lt;&lt; <xsl:value-of select="@expr" />;
</xsl:template>
<xsl:template mode="execContext" match="s:assign"><xsl:call-template name="comment" />
<xsl:choose>

    <xsl:when test="string(@dataid) != ''">
                set_<xsl:value-of select="@dataid" />(<xsl:value-of select="@expr" />);
    </xsl:when>
    <xsl:when test="string(@location) != ''">
              <xsl:value-of select="@location" /> = (<xsl:value-of select="@expr" />);
    </xsl:when>
</xsl:choose>
</xsl:template>
<xsl:template mode="execContext" match="s:raise|s:send">
<xsl:call-template name="comment" />
            <xsl:choose>
                <xsl:when test="name()='raise' or @target='_internal'">postEvent(</xsl:when>
                <xsl:otherwise><xsl:if test="string(@id)!=''">
                 _eventSenders["<xsl:value-of select="@id" />"] = </xsl:if>new SCC_EventSender(this,<xsl:if test="string(@delay)=''">0</xsl:if>
                    <xsl:value-of select="@delay" />,
                            </xsl:otherwise>
                    </xsl:choose>
                            new QStateMachine::SignalEvent(<xsl:if
                        test="string(@target)=''">this</xsl:if><xsl:value-of
                        select="@target" />,metaObject()->indexOfSignal(QMetaObject::normalizedSignature("event_<xsl:value-of select="replace(@event,'\.','__')" /><xsl:if test="not(contains(@event,')'))">()</xsl:if>")),QVariantList()<xsl:for-each
                        select="s:param"><xsl:text><![CDATA[<<]]></xsl:text> QVariant(<xsl:choose>
                          <xsl:when test="string(@expr)!=''"><xsl:value-of select="@expr" /></xsl:when>
                          <xsl:when test="string(@name)!=''">get_<xsl:value-of select="@name" />()</xsl:when>
                          <xsl:otherwise><xsl:value-of select="text()" /></xsl:otherwise>
                        </xsl:choose>)
                      </xsl:for-each>)<xsl:if test="name()='raise' or @target='_internal'">,QStateMachine::HighPriority</xsl:if>);
</xsl:template>
<xsl:template mode="execContext" match="s:cancel">
<xsl:call-template name="comment" /><![CDATA[
                  {
                    QPointer<SCC_EventSender> es = _eventSenders["]]><xsl:value-of select="@id" />"];
                    if (es)
                      es->cancel();
                  }
</xsl:template>

<xsl:template match="/">
#ifndef __SMCLASS_<xsl:call-template name="smname" />_H
#define __SMCLASS_<xsl:call-template name="smname" />_H
#include "QStateMachine"
#include "QSignalTransition"
#include "QTimer"
#include "QMetaMethod"
#include "QPointer"
#include "QVariant"
<xsl:if test="count(//s:final)!=0">

 #include "QFinalState"
</xsl:if>
<xsl:if test="count(//s:history)!=0">
 #include "QHistoryState"
</xsl:if>
#include "QHash"
#include "QEventTransition"
<xsl:if test="count(//s:log)!=0">
#include "QDebug"
</xsl:if>
<xsl:if test="count(//s:transition[string(@Qt:animation)!=''])!=0">
#include "QPropertyAnimation"
</xsl:if>

#define In(state) (configuration().contains(state_##state))

<xsl:value-of select="/s:scxml/Qt:cpp/text()" />

class SMClass_<xsl:call-template name="smname" />;
    <xsl:if test="count(//s:transition[string(@event)='' and string(@cond)=''])!=0">
     class SCC_UnconditionalTransition : public QAbstractTransition
     {
     public:
         SCC_UnconditionalTransition(QState* s)
             : QAbstractTransition(s) {}
     protected:
         void onTransition(QEvent *) {}
         bool eventTest(QEvent *) { return true; }
     };
    </xsl:if>
    <xsl:if test="count(//s:send[string(@target)!='_internal'])!=0">
    <![CDATA[
      #define ARG_FROM_VAR(I) \
          (acount > I \
                ? QGenericArgument(((QStateMachine::SignalEvent*)event)->arguments()[I].typeName(),((QStateMachine::SignalEvent*)event)->arguments()[I].data()) \
                : QGenericArgument())
                

      class SCC_EventSender : public QTimer
      {
          Q_OBJECT
          private:
            QStateMachine* machine;
            QStateMachine::SignalEvent* event;
          public:
          SCC_EventSender(QStateMachine* m=NULL, int delay=0, QStateMachine::SignalEvent* e=NULL) : QTimer(m), machine(m), event(e)
          {
            setInterval(delay);
            setSingleShot(true);
            connect(this,SIGNAL(timeout()),this,SLOT(send()));
            start();
          }
          public Q_SLOTS:
            void cancel() { stop(); deleteLater(); }
            void send() { 
              QVariantList args = event->arguments();
              int acount = args.count();
              event->sender()->metaObject()->method(event->signalIndex()).invoke(event->sender(),
                          ARG_FROM_VAR(0),ARG_FROM_VAR(1),ARG_FROM_VAR(2),ARG_FROM_VAR(3),ARG_FROM_VAR(4),
                          ARG_FROM_VAR(5),ARG_FROM_VAR(6),ARG_FROM_VAR(7),ARG_FROM_VAR(8),ARG_FROM_VAR(9));
              deleteLater();
            }
      };
    ]]></xsl:if>
    <xsl:if test="count(//s:transition[string(@cond) != '' or @event='*']) !=0">
namespace {
    <xsl:for-each select="//s:transition[string(@cond) != '' or @event='*']">
          <xsl:call-template name="comment" />
        class Transition_<xsl:value-of select="generate-id()" /> : public QSignalTransition
        {
            SMClass_<xsl:call-template name="smname" />* stateMachine;
            public:
                Transition_<xsl:value-of select="generate-id()" />(QState* parent)
                    : QSignalTransition(parent->machine(),<xsl:choose><xsl:when test="@event='*'">SIGNAL(destroyed())</xsl:when><xsl:otherwise>SIGNAL(event_<xsl:value-of select="@event" />)</xsl:otherwise></xsl:choose>,parent)
                      ,stateMachine((SMClass_<xsl:call-template name="smname" />*)parent->machine())
                {
                }

            protected:
                bool eventTest(QEvent* e);
        };

    </xsl:for-each>
};
    </xsl:if>
class SMClass_<xsl:call-template name="smname" /> : public QStateMachine
{
    Q_OBJECT
<xsl:for-each select="//s:datamodel/s:data">
 <xsl:call-template name="comment" />
    Q_PROPERTY(<xsl:value-of select="concat(@Qt:type,' ')" /> <xsl:value-of select="@id" /> READ get_<xsl:value-of select="@id" /> WRITE set_<xsl:value-of select="@id" /> NOTIFY <xsl:value-of select="@id" />_changed)
</xsl:for-each>
<xsl:for-each select="//s:transition/Qt:animation">
 <xsl:call-template name="comment" />
    Q_PROPERTY(QPropertyAnimation* <xsl:value-of select="string()" /> READ anim_<xsl:value-of select="string()" /> WRITE setAnim_<xsl:value-of select="string()" />)
</xsl:for-each>

    public:
        SMClass_<xsl:call-template name="smname" />(QObject* o = NULL)
            : QStateMachine(o)
              {
<xsl:for-each select="//s:datamodel/s:data[string-length(concat(string(text()),string(@expr)))!=0]">
          <xsl:call-template name="comment" />
    <xsl:if test="string(@expr) != ''">
            _data.<xsl:value-of select="@id" /> = <xsl:value-of select="@expr"/><xsl:value-of select="text()"/>;
    </xsl:if>
</xsl:for-each>
              }
    <xsl:for-each select="//s:state|//s:parallel|//s:initial">
          <xsl:call-template name="comment" />
        QState* <xsl:apply-templates mode="stateid" select="." />;</xsl:for-each>
    <xsl:for-each select="//s:final">
          <xsl:call-template name="comment" />
        QFinalState* <xsl:apply-templates mode="stateid" select="." />;
    </xsl:for-each>
    <xsl:for-each select="//s:history">
          <xsl:call-template name="comment" />
        QHistoryState* <xsl:apply-templates mode="stateid" select="." />;
    </xsl:for-each>
    <xsl:for-each select="//s:transition[string(@cond) != '']">
          <xsl:call-template name="comment" />
        inline bool testCondition_<xsl:value-of select="generate-id()" />()
        {
            return <xsl:if test="string(@cond)=''">true</xsl:if><xsl:value-of select="@cond"/>;
        }
    </xsl:for-each>
    <xsl:for-each select="//s:datamodel/s:data">
          <xsl:call-template name="comment" />
        <xsl:text>        </xsl:text><xsl:value-of select="@Qt:type" /> get_<xsl:value-of select="@id" />() const
        {
            return _data.<xsl:value-of select="@id" />;
        }
    </xsl:for-each>
      <xsl:if test="count(//s:datamodel/s:data) != 0"><![CDATA[
    protected:
        struct {
    ]]><xsl:for-each select="//s:datamodel/s:data">
          <xsl:call-template name="comment" />
            <xsl:text>              </xsl:text><xsl:value-of select="@Qt:type" /><xsl:text> </xsl:text><xsl:value-of select="@id" />;
            </xsl:for-each>
        } _data;
           </xsl:if>
        struct {
          QString name;
          QVariantList data;
        } _event;
        QString _name;
             <xsl:if test="(count(//s:datamodel/s:data)+count(/s:scxml/s:script))!=0">
    public Q_SLOTS:
</xsl:if>
<xsl:for-each select="//s:datamodel/s:data">
          <xsl:call-template name="comment" />
        void set_<xsl:value-of select="@id" />(<xsl:value-of select="@Qt:type" /> const &amp; value)
        {
            _data.<xsl:value-of select="@id" /> = value;
            emit <xsl:value-of select="@id" />_changed(value);
        }
    </xsl:for-each>
    private Q_SLOTS:
#ifndef QT_NO_PROPERTIES
        void assignProperties()
        {
        <xsl:for-each select="//Qt:property">
          <xsl:call-template name="comment" />
<xsl:text>        </xsl:text><xsl:apply-templates mode="stateid" select=".." />->assignProperty(<xsl:value-of
                    select="@object" />,"<xsl:value-of select="@property" />",QVariant(<xsl:value-of select="@value" />));
        </xsl:for-each>
#endif
        }
     <xsl:for-each select="//s:transition|//s:onentry|//s:onexit"><xsl:if test="count(*) != 0">
          <xsl:for-each select="..|."><xsl:call-template name="comment" /></xsl:for-each>
        void exec_<xsl:value-of select="generate-id()" />()
        {
            <xsl:apply-templates mode="execContext" />
        }
    </xsl:if></xsl:for-each>
   Q_SIGNALS:<xsl:for-each select="distinct-values(//node()[not (starts-with(@event,'done.state.') or @event='*' or contains(@event,':'))]/@event)">
        void event_<xsl:value-of select="replace(string(),'\.','__')" /><xsl:if test="not(contains(string(),')'))">()</xsl:if>;</xsl:for-each>
    <xsl:for-each select="//s:datamodel/s:data">
          <xsl:call-template name="comment" />
        void <xsl:value-of select="@id" />_changed(<xsl:value-of select="@Qt:type" /> const &amp;);
    </xsl:for-each>
      <xsl:text><![CDATA[
    protected:
    virtual void beginSelectTransitions(QEvent *event)
    {
        if (event && !event->type() == QEvent::None) {
          switch (event->type()) {
            case QEvent::StateMachineSignal: {
              QStateMachine::SignalEvent* e = (QStateMachine::SignalEvent*)event;
              _event.data = e->arguments();
              _event.name = e->sender()->metaObject()->method(e->signalIndex()).signature();
              if (e->sender() == this)
                _event.name = _event.name.mid(6);
            } break;
            default:
            break;
          }
        } else {
          _event.name = "";
          _event.data.clear();
        }
 ]]>        
        assignProperties();
    }
</xsl:text>
<xsl:if test="count(//s:send[string(@target)!='internal']) != 0">
    private:
        <xsl:text><![CDATA[QHash<QString,QPointer<SCC_EventSender> >]]></xsl:text> _eventSenders;
</xsl:if>
    protected:
    public:
        void setupStateMachine()
        {
            _name = "<xsl:call-template name="smname" />";
            setObjectName(_name);
            <xsl:for-each select="//s:state|//s:parallel|//s:final|//s:history|//s:initial">
            <xsl:call-template name="comment" />
            <xsl:text>            </xsl:text><xsl:apply-templates mode="stateid" select="." /> = new <xsl:choose>
                    <xsl:when test="name()='final'">QFinalState</xsl:when>
                    <xsl:when test="name()='history'">QHistoryState</xsl:when>
                    <xsl:otherwise>QState</xsl:otherwise>
                </xsl:choose>(<xsl:apply-templates mode="stateid" select=".." />);
            <xsl:if test="string(@id)!=''"><xsl:apply-templates mode="stateid" select="." />->setObjectName("<xsl:value-of select="@id" />");</xsl:if>
            <xsl:if
               test="name()='initial' or @id=../@initial">
            <xsl:apply-templates mode="stateid" select=".." />->setInitialState(<xsl:apply-templates mode="stateid" select="." />);
            </xsl:if>
               <xsl:if test="name()='parallel'">
                    <xsl:apply-templates mode="stateid" select="." />->setChildMode(ParallelStates);
            </xsl:if>
               <xsl:if test="name()='history'">
             <xsl:apply-templates mode="stateid" select="." />->setHistoryType(QHistoryState::<xsl:choose>
                       <xsl:when test="@type='deep'">Deep</xsl:when>
                       <xsl:otherwise>Shallow</xsl:otherwise>
                       </xsl:choose>History);
            </xsl:if>
            </xsl:for-each>
            QAbstractTransition* transition;<xsl:for-each select="//s:transition[../name()!='history']">
          <xsl:call-template name="comment" />
            transition = new <xsl:choose>
            <xsl:when test="@event='*' or string(@cond)!=''">Transition_<xsl:value-of
             select="generate-id()" />(</xsl:when>
             <xsl:when test="starts-with(@event,'q-event:')">QEventTransition(<xsl:value-of
              select="substring-after(@event,'q-event:')" />,</xsl:when>
             <xsl:when test="starts-with(@event,'done.state.')">QSignalTransition(state_<xsl:value-of
              select="substring-after(@event,'done.state.')" />,SIGNAL(finished()),</xsl:when>
             <xsl:when test="string(@event)!=''">QSignalTransition(this,SIGNAL(event_<xsl:value-of select="replace(@event,'\.','__')" /><xsl:if test="not(ends-with(@event,')'))">()</xsl:if>),</xsl:when>
             <xsl:otherwise>SCC_UnconditionalTransition(</xsl:otherwise></xsl:choose><xsl:apply-templates mode="stateid" select=".." />);<xsl:if test="count(*) != 0">
            connect(transition,SIGNAL(triggered()),this,SLOT(exec_<xsl:value-of select="generate-id()" />()));</xsl:if>
            <xsl:if test="string(@Qt:animation) != ''">
            transition->addAnimation(<xsl:value-of select="@Qt:animation" />);</xsl:if><xsl:if
                  test="string(@target) != ''"></xsl:if>
            <xsl:choose>
                <xsl:when test="string(@target)=''" />
                <xsl:when test="count(tokenize(@target,'\s+'))=1">
            transition->setTargetState(state_<xsl:value-of select="@target" />);</xsl:when>
                <xsl:otherwise>(*transition).setTargetStates(QList&lt;QAbstractState*&gt;()<xsl:for-each
                    select="tokenize(@target,'\s+')"> &lt;&lt; state_<xsl:value-of select="." /></xsl:for-each>);</xsl:otherwise>
            </xsl:choose>
            </xsl:for-each>
            <xsl:for-each select="//s:history/s:transition">
         <xsl:apply-templates mode="stateid" select=".." />->setDefaultState(state_<xsl:value-of select="@target" />);
                  </xsl:for-each>
            <xsl:for-each select="//s:onentry|//s:onexit"><xsl:if test="count(*) != 0">
            connect(<xsl:apply-templates mode="stateid" select=".." />, SIGNAL(<xsl:choose>
                   <xsl:when test="name()='onentry'">entered</xsl:when>
                   <xsl:when test="name()='onexit'">exited</xsl:when>
                </xsl:choose>()),this,SLOT(exec_<xsl:value-of select="generate-id()" />()));</xsl:if></xsl:for-each>
            <xsl:value-of select="/s:scxml/s:script/text()" />
        }
};
    <xsl:for-each select="//s:transition[@event='*' or string(@cond) != '']">
      <xsl:call-template name="comment" />
        bool Transition_<xsl:value-of select="generate-id()" />::eventTest(QEvent* e)
        {
            return <xsl:choose><xsl:when test="@event!='*'">Q<xsl:if test="string(@event) != ''">Signal</xsl:if>Transition::eventTest(e)
            </xsl:when><xsl:otherwise>(*e).type() != QEvent::None</xsl:otherwise></xsl:choose><xsl:if test="string(@cond)!=''">&amp;&amp; stateMachine-&gt;testCondition_<xsl:value-of 
                  select="generate-id()" />()</xsl:if>;
        }
    </xsl:for-each>
#endif
    </xsl:template>

</xsl:stylesheet>
