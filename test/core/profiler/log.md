## 2018/3/11
### DEBUG������ƿ����
* playout() -> 92.01%
  * simulate() -> 71.14%
    * getRandomMove() -> 40.65%
	* applyMove() -> 28.26%
  * reset() -> 18.69%
  * expand() -> 0.18%

* �ܼƣ�applyMove() -> 34.48%
  * checkMove()��DEBUG�棩 -> 2.18%
  * hashset[xxx] = xxx -> 6.90%
  * **hashset.erase() -> 11.43%**
  * **checkVictory() -> 13.79%**
    * ����ȫ�����������´����У�
	  ```cpp
		(x >= 0 && x < width) && (y >= 0 && y < height) 
        && (m_appliedMoves.count(pose) && m_appliedMoves[pose] == m_curPlayer);
	  ```

* �ܼƣ�getRandomMove() -> 41.38%
  * **hashset.bucket_size() -> 36.30%**
  * bucket = (bucket + 1) % hashset.bucket_count() -> 2.54%
  * hashset.begin(bucket), rand () % bucketSize -> 1.63


**�ܽ�**��DEBUGģʽ�£�STL�Ŀ�����Ϊ�˾��ԵĴ�ͷ��



## 2018/3/12

### ������������Ϊstd::array�����޸�һЩBUG��RELEASE�µ����ܷ�����

�����`2018/3/11`�汾������������Լ50����
��$10^6$��playout�£�һ�β��Ժ�ʱԼ4�롣

Ϊ�˻�ȡ����ϸ�ĺ����������������ǹر��˱���ʱ������չ���ٽ�����һ�β��ԣ�
����������һЩ�������ֹ������չ��ʹ�ÿ�������ĺ�����Ŀǰ��CPU�����ֲ�Ϊ��

* playout() -> 98.45%
* * applyMove() -> 50.50%
  * reset() -> 23.18%
  * select() -> 10.47% ����ֹ����չ������Ŀ�����
* �ܼƣ�applyMove() -> 50.61%
  * checkVictory() -> 39.58%
    * ��`2018/3/11`�汾һ���������Ծɼ�����isCurrentPlayer()�����С�
  * setState() -> 2.78%
  * unsetState() -> 1.82%
  * checkMove() -> 1.19%
* �ܼƣ�reset() -> 23.18%
  * deque::back() -> 13.02%
  * revertMove() -> 7.21%
    * setState() -> 2.53%
    * unsetState() -> 2.14%


**�ܽ�**��

1. ����std::array�󣬾������ܵõ����ʵ���������applyMove()��revertMove()�Ծ���������ͷ��

2. ֵ��ע����ǣ���Ŀǰ��ʵ���У�checkMove()��isCurrentPlayer()�������¼���һģһ�������俪����ռ��ȴ����33��֮�ࡣ

   ���ܵ�ԭ���ǣ���ÿ��applyMove()�У�checkMove()��checkVictory()��������һ�Σ���checkVictory����4������ÿ�������������2*4���ӣ�Ҳ����isCurrentPlayer()��߻ᱻ����32�Σ�����˾޴�Ŀ�����

3. ���⣬����ȡջ��Ԫ�ص�deque::back()Ҳֵ��ע�⡣���ܽ�ֹ������չ�������俪�������Եù����ˡ�


**���ܵ����ܽ���**��

1. ����Ŀǰ�汾��isCurrentPlayer()ֻ��һ�����뱻���ã���˿��Խ���Ӻ�����װ�в������
2. ��һ���Ż�ʤ�������㷨������isCurrentPlayer()�Ȳ�����Ƶ������
3. ��std::stack��Container��Ϊstd::vector����ɴ�ͽ�std::vector��Ϊջʹ�á�



### ��MCTS��playout���ƽ�һ�����ź�RELEASE�����ܷ�����

��$10^6$��playout�£���ʱԼ950ms���������һ���汾������������4����
Ҳ����˵�����������İ汾����������������200����

δ��ֹ������չʱ��CPU�����ֲ������

* playout() -> 99.75%
  * backPropogate() -> 43.18%
    * ���������revertMove()
  * applyMove() ->  39.43%
  * checkGameEnd() -> 7.86%
  * select() -> 7.44%
  * Node::isLeaf() -> 1.44%

��ֹ������չ�󣬾���Profiler���ԣ�����applyMove()��revertMove()�Ĵ�ͷ�����������������Ż�����ɵġ�
�����˵����Ŀǰ���Դ�����·��Ż��ܴ�����������ʱ�����ˡ�